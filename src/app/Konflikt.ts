import { Console } from "./Console";
import { InstanceRole } from "./InstanceRole";
import { KonfliktNative as KonfliktNativeConstructor } from "../native/native";
import { LayoutManager } from "./LayoutManager";
import { PeerManager } from "./PeerManager";
import { Rect } from "./Rect";
import { Server } from "./Server";
import { createHash } from "crypto";
import { createNativeLogger } from "../native/createNativeLogger";
import { createPromise } from "./createPromise";
import { debug } from "./debug";
import { error } from "./error";
import { hostname, platform } from "os";
import { isClientRegistrationMessage } from "./isClientRegistrationMessage";
import { isLayoutAssignmentMessage } from "./isLayoutAssignmentMessage";
import { isLayoutUpdateMessage } from "./isLayoutUpdateMessage";
import { log } from "./log";
import { setLogBroadcaster } from "./logBroadcaster";
import { verbose } from "./verbose";
import type { ClientRegistrationMessage } from "./ClientRegistrationMessage";
import type { Config } from "./Config";
import type { ConnectedInstanceInfo } from "./ConnectedInstanceInfo";
import type { DiscoveredService } from "./DiscoveredService";
import type { InputEventData } from "./InputEventData";
import type { InputEventMessage } from "./InputEventMessage";
import type { InputEventType } from "./InputEventType";
import type { InstanceInfoMessage } from "./InstanceInfoMessage";
import type { KeyPressEvent } from "./KeyPressEvent";
import type { KeyReleaseEvent } from "./KeyReleaseEvent";
import type {
    KonfliktDesktopEvent,
    KonfliktKeyPressEvent,
    KonfliktKeyReleaseEvent,
    KonfliktMouseButtonPressEvent,
    KonfliktMouseButtonReleaseEvent,
    KonfliktMouseMoveEvent,
    KonfliktNative
} from "../native/KonfliktNative";
import type { LayoutAssignmentMessage } from "./LayoutAssignmentMessage";
import type { LayoutUpdateMessage } from "./LayoutUpdateMessage";
import type { Message } from "./Message";
import type { MouseMoveEvent } from "./MouseMoveEvent";
import type { MousePressEvent } from "./MousePressEvent";
import type { MouseReleaseEvent } from "./MouseReleaseEvent";
import type { NetworkMessage } from "./NetworkMessage";
import type { Point } from "./Point";
import type { PreferredPosition } from "./PreferredPosition";
import type { PromiseData } from "./PromiseData";
import type { ScreenEntry } from "./ScreenEntry";
import type { ScreenInfo } from "./ScreenInfo";

export class Konflikt {
    #config: Config;
    #native: KonfliktNative;
    #server: Server;
    #peerManager: PeerManager | undefined;
    #console: Console | undefined;
    #layoutManager: LayoutManager | undefined;

    // Role-based behavior
    #role: InstanceRole;
    #isActiveInstance: boolean = false;
    #lastCursorPosition: Point = { x: 0, y: 0 };
    #virtualCursorPosition: Point | null = null; // Virtual cursor for remote screen
    #activeRemoteScreenBounds: Rect | null = null; // Bounds of the currently active remote screen
    #screenBounds: Rect;
    #lastDeactivationRequest: number = 0; // Debounce for deactivation requests
    #activatedClientId: string | null = null; // Track which client we've activated at the edge
    #run: PromiseData<void>;
    #displayId: string;
    #machineId: string;
    #connectedInstances: Map<string, ConnectedInstanceInfo> = new Map();

    constructor(config: Config) {
        this.#run = createPromise();
        this.#config = config;
        this.#role = config.role;
        this.#native = new KonfliktNativeConstructor(createNativeLogger());
        this.#server = new Server(
            config.port,
            this.#run.resolve,
            config.instanceId,
            config.instanceName,
            "1.0.0",
            ["input_events", "state_sync"],
            this.#role === InstanceRole.Server ? "server" : "client"
        );

        // Generate machine identifier
        this.#machineId = Konflikt.#generateMachineId();

        // Create LayoutManager for servers
        if (this.#role === InstanceRole.Server) {
            this.#layoutManager = new LayoutManager();
            this.#server.layoutManager = this.#layoutManager;
        }

        // Calculate screen bounds from config
        const desktop = this.#native.desktop;
        this.#screenBounds = new Rect(
            config.screenX,
            config.screenY,
            config.screenWidth ?? desktop.width,
            config.screenHeight ?? desktop.height
        );

        console.log(
            "Using screen bounds:",
            this.#screenBounds,
            desktop,
            this.#role === InstanceRole.Server ? "(server)" : "(client)"
        );

        // Generate display identifier after screen bounds are set
        this.#displayId = this.#generateDisplayId();

        // Only servers listen for input events
        if (this.#role === InstanceRole.Server) {
            verbose("Setting up input event listeners for server");
            this.#native.on("keyPress", this.#onKeyPress.bind(this));
            this.#native.on("keyRelease", this.#onKeyRelease.bind(this));
            this.#native.on("mousePress", this.#onMousePress.bind(this));
            this.#native.on("mouseRelease", this.#onMouseRelease.bind(this));
            this.#native.on("mouseMove", this.#onMouseMove.bind(this));
        }

        // Desktop changes are relevant for both servers and clients
        this.#native.on("desktopChanged", this.#onDesktopChanged.bind(this));
    }

    get config(): Config {
        return this.#config;
    }

    get native(): KonfliktNative {
        return this.#native;
    }

    get server(): Server {
        return this.#server;
    }

    get console(): Console | undefined {
        return this.#console;
    }

    get role(): InstanceRole {
        return this.#role;
    }

    get isActiveInstance(): boolean {
        return this.#isActiveInstance;
    }

    get screenBounds(): Rect {
        return this.#screenBounds.clone();
    }

    get lastCursorPosition(): { x: number; y: number } {
        return { ...this.#lastCursorPosition };
    }

    get displayId(): string {
        return this.#displayId;
    }

    get machineId(): string {
        return this.#machineId;
    }

    get connectedInstancesCount(): number {
        return this.#connectedInstances.size;
    }

    get hasSameDisplayInstances(): boolean {
        return this.#hasSameDisplayInstance();
    }

    async init(): Promise<void> {
        verbose("Initializing Konflikt...", this.#config.port);

        // Pass config to server for console commands
        this.#server.config = this.#config;

        // Set up message handler for input events
        this.#server.setMessageHandler(this.handleNetworkMessage.bind(this));

        await this.#server.start();

        // Set up service discovery event handlers for all roles
        this.#server.serviceDiscovery.on("serviceUp", (service: DiscoveredService) => {
            if (this.#role === InstanceRole.Server && service.txt?.role === "server") {
                // Skip our own service
                if (service.port === this.#server.port) {
                    return;
                }
                // Check if this is on the same host (localhost, 127.0.0.1, current hostname, or our actual hostname)
                const isSameHost =
                    service.host === "localhost" ||
                    service.host === "127.0.0.1" ||
                    service.host === hostname() ||
                    service.addresses.includes("127.0.0.1") ||
                    service.addresses.includes("::1");

                if (isSameHost) {
                    // Compare start timestamps to determine which server should quit
                    const discoveredStartTime = parseInt((service.txt!.started as string) || "0", 10);
                    const ourStartTime = this.#server.startTime;

                    if (discoveredStartTime < ourStartTime) {
                        // The discovered server started earlier - we are newer, so tell it to quit
                        log(
                            `Discovered older server at ${service.host}:${service.port} (started: ${new Date(discoveredStartTime).toISOString()})`
                        );
                        verbose("Requesting older server to quit so this newer instance can take over...");
                        Konflikt.#requestServerQuit(service);
                    } else if (discoveredStartTime > ourStartTime) {
                        // The discovered server is newer - it should tell us to quit eventually
                        verbose(
                            `Discovered newer server at ${service.host}:${service.port} (started: ${new Date(discoveredStartTime).toISOString()})`
                        );
                        verbose("Newer server should request this instance to quit shortly...");
                    } else {
                        // Same start time (very unlikely) - use PID as tiebreaker
                        const discoveredPid = parseInt((service.txt!.pid as string) || "0", 10);
                        if (discoveredPid < process.pid) {
                            verbose(
                                `Server collision with same start time, using PID tiebreaker. Requesting server ${discoveredPid} to quit...`
                            );
                            Konflikt.#requestServerQuit(service);
                        }
                    }
                } else {
                    verbose(`Discovered server on different host: ${service.name} at ${service.host}:${service.port}`);
                }
                return;
            }

            if (this.#role === InstanceRole.Client && service.txt?.role === "server") {
                verbose(`Client discovered server: ${service.name} at ${service.host}:${service.port}`);
                this.#connectToServer(service);
            }
        });

        // Role-specific initialization
        if (this.#role === InstanceRole.Server) {
            verbose(`Server ${this.#config.instanceId} is now ready to capture input events`);
            verbose(`Screen bounds: ${JSON.stringify(this.#screenBounds)}`);

            // Register server's own screen in layout manager
            if (this.#layoutManager) {
                this.#layoutManager.setServerScreen(
                    this.#config.instanceId,
                    this.#config.instanceName,
                    this.#machineId,
                    this.#screenBounds.width,
                    this.#screenBounds.height
                );

                // Listen for layout changes to broadcast to clients
                this.#layoutManager.on("layoutChanged", (screens: ScreenEntry[]) => {
                    const layoutUpdate: LayoutUpdateMessage = {
                        type: "layout_update",
                        screens: screens.map((s: ScreenEntry) => ({
                            instanceId: s.instanceId,
                            displayName: s.displayName,
                            x: s.x,
                            y: s.y,
                            width: s.width,
                            height: s.height,
                            isServer: s.isServer,
                            online: s.online
                        })),
                        timestamp: Date.now()
                    };
                    this.#server.broadcastToClients(JSON.stringify(layoutUpdate));
                });
            }

            const adjacency = this.#config.adjacency;
            if (Object.keys(adjacency).length > 0) {
                verbose(`Screen adjacency configuration: ${JSON.stringify(adjacency, null, 2)}`);
            }
        } else {
            // Client role: set up peer manager to connect to servers
            verbose(`Client ${this.#config.instanceId} initializing - looking for servers to connect to`);

            this.#peerManager = new PeerManager(this.#config.instanceId, this.#config.instanceName, "1.0.0", [
                "input_events",
                "screen_info"
            ]);

            // Set up peer connection event handlers
            this.#setupPeerConnectionHandlers();

            // If --server was specified, connect directly (don't wait for mDNS)
            const serverHost = this.#config.serverHost;
            const serverPort = this.#config.serverPort;
            if (serverHost) {
                log(`Connecting to configured server: ${serverHost}:${serverPort}`);
                const directService: DiscoveredService = {
                    name: `server-${serverHost}`,
                    host: serverHost,
                    port: serverPort || 3000,
                    addresses: [],
                    txt: { role: "server" }
                };
                this.#connectToServer(directService);
            }
        }

        // Send initial instance info and set up periodic broadcasting
        this.#sendInstanceInfo();
        setInterval(() => {
            this.#sendInstanceInfo();
        }, 10000); // Send instance info every 10 seconds

        // Set up log broadcasting to remote consoles
        setLogBroadcaster((level: "verbose" | "debug" | "log" | "error", message: string) => {
            this.#server.console.broadcastLogMessage(level, message);
        });

        // Start console based on configuration
        const consoleConfig = this.#config.console;
        if (consoleConfig === true) {
            try {
                if (process.stdin.isTTY && process.stdout.isTTY) {
                    this.#console = new Console(this);
                    this.#console.start();
                } else {
                    verbose("Non-interactive environment detected, console disabled");
                }
            } catch (err) {
                verbose("Console initialization failed:", err);
                // Continue without console
            }
        } else if (consoleConfig === false) {
            verbose("Console disabled by configuration");
        }
        // Remote console mode is handled in index.ts and doesn't reach here
    }

    run(): Promise<void> {
        verbose("Running Konflikt instance...", this.#config);
        return this.#run.promise;
    }

    quit(): void {
        verbose("Quitting Konflikt instance...");
        this.#run.resolve();
    }

    #getTargetClientForPosition(x: number, y: number): string | null {
        // For servers: determine which connected client should receive input at this position
        if (this.#role !== InstanceRole.Server || !this.#layoutManager) {
            return null;
        }

        // If cursor is within server's screen bounds, don't forward to clients
        if (this.#screenBounds.contains({ x, y })) {
            return null;
        }

        // Check if cursor has transitioned to another screen
        const transition = this.#layoutManager.getTransitionTarget(this.#config.instanceId, x, y);
        if (transition) {
            return transition.targetScreen.instanceId;
        }

        return null;
    }

    /**
     * Check if cursor should transition to another screen and handle the transition.
     * The cursor can't actually go outside the screen bounds (OS clamps it),
     * so we check if it's at an edge instead.
     */
    #checkScreenTransition(x: number, y: number): boolean {
        if (this.#role !== InstanceRole.Server || !this.#layoutManager) {
            return false;
        }

        const EDGE_THRESHOLD = 1; // Cursor is at edge if within 1 pixel of boundary

        // Determine which edge (if any) the cursor is at
        let edge: "left" | "right" | "top" | "bottom" | null = null;

        if (x <= this.#screenBounds.x + EDGE_THRESHOLD) {
            edge = "left";
        } else if (x >= this.#screenBounds.x + this.#screenBounds.width - EDGE_THRESHOLD - 1) {
            edge = "right";
        } else if (y <= this.#screenBounds.y + EDGE_THRESHOLD) {
            edge = "top";
        } else if (y >= this.#screenBounds.y + this.#screenBounds.height - EDGE_THRESHOLD - 1) {
            edge = "bottom";
        }

        if (!edge) {
            // Cursor is not at an edge - clear any previous activation
            // BUT only if we don't have a virtual cursor (remote screen active)
            if (this.#virtualCursorPosition === null && this.#activatedClientId !== null) {
                verbose(`Clearing activatedClientId (was ${this.#activatedClientId}) - cursor not at edge`);
                this.#activatedClientId = null;
            }
            return false;
        }

        const transition = this.#layoutManager.getTransitionTargetAtEdge(this.#config.instanceId, edge, x, y);
        if (!transition) {
            return false;
        }

        // Only send activation once per edge touch
        if (this.#activatedClientId === transition.targetScreen.instanceId) {
            return true; // Already activated this client, don't spam
        }

        log(
            `Screen transition: cursor at edge '${edge}' (${x}, ${y}) -> ${transition.targetScreen.displayName} at (${transition.newX}, ${transition.newY})`
        );

        // Send activation message to the target client
        this.#activateClient(transition.targetScreen.instanceId, transition.newX, transition.newY);

        return true;
    }

    /**
     * Send activation message to a client to make it the active input receiver
     */
    #activateClient(targetInstanceId: string, cursorX: number, cursorY: number): void {
        // Set this FIRST before anything else to avoid race conditions
        this.#activatedClientId = targetInstanceId;

        const message = {
            type: "activate_client",
            targetInstanceId,
            cursorX,
            cursorY,
            timestamp: Date.now()
        };

        this.#server.broadcastToClients(JSON.stringify(message));
        log(`Sent activation to ${targetInstanceId} at (${cursorX}, ${cursorY}), activatedClientId=${this.#activatedClientId}`);

        // Initialize virtual cursor for the remote screen
        this.#virtualCursorPosition = { x: cursorX, y: cursorY };

        // Store the remote screen bounds for clamping
        const targetScreen = this.#layoutManager?.getScreen(targetInstanceId);
        if (targetScreen) {
            this.#activeRemoteScreenBounds = new Rect(0, 0, targetScreen.width, targetScreen.height);
            verbose(`Activated remote screen ${targetInstanceId}, bounds: ${targetScreen.width}x${targetScreen.height}`);
        } else {
            // Fallback to a reasonable default if screen info not available
            error(`No screen info for ${targetInstanceId}, using default bounds`);
            this.#activeRemoteScreenBounds = new Rect(0, 0, 1920, 1080);
        }

        // Hide the cursor on the server since we're controlling a remote screen
        log(`Hiding server cursor`);
        this.#native.hideCursor();
        log(`Server cursor hidden: ${!this.#native.isCursorVisible()}`);

        // Server is no longer the active instance
        this.#isActiveInstance = false;
    }

    /**
     * Deactivate the remote screen and return control to the server
     */
    #deactivateRemoteScreen(): void {
        log(`Deactivating remote screen, returning to server`);

        // Clear virtual cursor state
        this.#virtualCursorPosition = null;
        this.#activeRemoteScreenBounds = null;
        this.#activatedClientId = null;

        // Show the cursor on the server again
        log(`Showing server cursor`);
        this.#native.showCursor();
        log(`Server cursor visible: ${this.#native.isCursorVisible()}`);

        // Server is now the active instance again
        this.#isActiveInstance = true;

        // TODO: Send deactivation message to the client so it knows to stop showing cursor
    }

    // Method to be called by Server when receiving network messages
    handleNetworkMessage(message: NetworkMessage): void {
        if (message.type === "input_event") {
            this.#handleInputEvent(message);
        } else if (message.type === "instance_info") {
            this.#handleInstanceInfo(message);
        } else if (isClientRegistrationMessage(message)) {
            this.#handleClientRegistration(message);
        } else if (isLayoutAssignmentMessage(message)) {
            this.#handleLayoutAssignment(message);
        } else if (isLayoutUpdateMessage(message)) {
            this.#handleLayoutUpdate(message);
        } else if (Konflikt.#isActivateClientMessage(message)) {
            this.#handleActivateClient(message);
        } else if (Konflikt.#isDeactivationRequestMessage(message)) {
            this.#handleDeactivationRequest(message);
        } else {
            verbose(`Unknown message type: ${JSON.stringify(message)}`);
        }
    }

    static #isDeactivationRequestMessage(
        message: unknown
    ): message is { type: "deactivation_request"; instanceId: string } {
        return (
            typeof message === "object" &&
            message !== null &&
            "type" in message &&
            message.type === "deactivation_request" &&
            "instanceId" in message
        );
    }

    #handleDeactivationRequest(message: { type: "deactivation_request"; instanceId: string }): void {
        // Only servers handle deactivation requests
        if (this.#role !== InstanceRole.Server) {
            return;
        }

        // Verify this is from the currently active client
        if (message.instanceId !== this.#activatedClientId) {
            verbose(`Ignoring deactivation request from ${message.instanceId} - active client is ${this.#activatedClientId}`);
            return;
        }

        log(`Received deactivation request from client ${message.instanceId}`);
        this.#deactivateRemoteScreen();
    }

    static #isActivateClientMessage(
        message: unknown
    ): message is { type: "activate_client"; targetInstanceId: string; cursorX: number; cursorY: number } {
        return (
            typeof message === "object" &&
            message !== null &&
            "type" in message &&
            message.type === "activate_client" &&
            "targetInstanceId" in message &&
            "cursorX" in message &&
            "cursorY" in message
        );
    }

    #handleActivateClient(message: {
        type: "activate_client";
        targetInstanceId: string;
        cursorX: number;
        cursorY: number;
    }): void {
        // Check if this activation is for us
        if (message.targetInstanceId !== this.#config.instanceId) {
            // Not for us - deactivate if we were active
            if (this.#isActiveInstance) {
                verbose(`Deactivating - another client (${message.targetInstanceId}) is now active`);
                this.#isActiveInstance = false;
            }
            return;
        }

        log(`Activating client at cursor position (${message.cursorX}, ${message.cursorY})`);
        this.#isActiveInstance = true;

        // Move cursor to the specified position using a mouse move event
        try {
            this.#native.sendMouseEvent({
                type: "mouseMove",
                x: message.cursorX,
                y: message.cursorY,
                timestamp: Date.now(),
                keyboardModifiers: 0,
                mouseButtons: 0,
                dx: 0,
                dy: 0
            });
            verbose(`Moved cursor to (${message.cursorX}, ${message.cursorY})`);
        } catch (err) {
            error(`Failed to move cursor: ${err}`);
        }
    }

    #handleClientRegistration(message: ClientRegistrationMessage): void {
        if (this.#role !== InstanceRole.Server || !this.#layoutManager) {
            return;
        }

        verbose(
            `Received client registration: ${message.displayName} (${message.instanceId}) - ${message.screenWidth}x${message.screenHeight}`
        );

        // Register the client in the layout manager
        const entry = this.#layoutManager.registerClient(
            message.instanceId,
            message.displayName,
            message.machineId,
            message.screenWidth,
            message.screenHeight
        );

        // Send layout assignment back to the client
        const layoutAssignment: LayoutAssignmentMessage = {
            type: "layout_assignment",
            position: { x: entry.x, y: entry.y },
            adjacency: this.#layoutManager.getAdjacencyFor(message.instanceId),
            fullLayout: this.#layoutManager.getLayout().map((s: ScreenEntry) => ({
                instanceId: s.instanceId,
                displayName: s.displayName,
                x: s.x,
                y: s.y,
                width: s.width,
                height: s.height,
                isServer: s.isServer,
                online: s.online
            }))
        };

        // TODO: Send to specific client - for now broadcast
        this.#server.broadcastToClients(JSON.stringify(layoutAssignment));
    }

    #handleLayoutAssignment(message: LayoutAssignmentMessage): void {
        if (this.#role !== InstanceRole.Client) {
            return;
        }

        verbose(`Received layout assignment: position (${message.position.x}, ${message.position.y})`);
        verbose(`Layout has ${message.fullLayout.length} screens`);

        // Update local screen position
        this.#screenBounds = new Rect(
            message.position.x,
            message.position.y,
            this.#screenBounds.width,
            this.#screenBounds.height
        );
    }

    #handleLayoutUpdate(message: LayoutUpdateMessage): void {
        if (this.#role !== InstanceRole.Client) {
            return;
        }

        verbose(`Received layout update with ${message.screens.length} screens`);

        // Find our screen in the layout and update position
        const ourScreen = message.screens.find((s: ScreenInfo) => s.instanceId === this.#config.instanceId);
        if (ourScreen) {
            this.#screenBounds = new Rect(ourScreen.x, ourScreen.y, ourScreen.width, ourScreen.height);
        }
    }

    // Active instance management methods
    #updateCursorPosition(x: number, y: number): void {
        this.#lastCursorPosition = { x, y };
        this.#checkIfShouldBeActive();
    }

    #checkIfShouldBeActive(): void {
        const wasPreviouslyActive = this.#isActiveInstance;
        verbose(
            `checkIfShouldBeActive: role=${this.#role === InstanceRole.Server ? "server" : "client"}, wasPreviouslyActive=${wasPreviouslyActive}, hasVirtualCursor=${this.#virtualCursorPosition !== null}`
        );

        if (this.#role === InstanceRole.Server) {
            // If we have a virtual cursor, a remote screen is active - don't override
            if (this.#virtualCursorPosition !== null) {
                verbose(`Server keeping remote screen active, virtual cursor at (${this.#virtualCursorPosition.x}, ${this.#virtualCursorPosition.y})`);
                return;
            }
            // Servers capture input events and send them to the current client
            this.#isActiveInstance = true;
            verbose(`Server set to active: ${this.#isActiveInstance}`);
        } else {
            // Clients receive input events from server and execute them locally
            // The server determines which client should receive input
            this.#isActiveInstance = false;
            verbose(`Client set to inactive: ${this.#isActiveInstance}`);
        }

        // Log state changes
        if (wasPreviouslyActive !== this.#isActiveInstance) {
            verbose(
                `Instance ${this.#config.instanceId} ${this.#isActiveInstance ? "became ACTIVE" : "became INACTIVE"} (role: ${this.#role === InstanceRole.Server ? "server" : "client"})`
            );
        }
    }

    #isCursorInScreenBounds(position: Point): boolean {
        return this.#screenBounds.contains(position);
    }

    #generateDisplayId(): string {
        // Create a display identifier based on desktop dimensions and position
        const desktop = this.#native.desktop;
        const displayInfo = `${desktop.width}x${desktop.height}-${this.#screenBounds.x},${this.#screenBounds.y}`;
        return createHash("sha256").update(`${this.#machineId}-${displayInfo}`).digest("hex").substring(0, 16);
    }

    #updateConnectedInstance(
        instanceId: string,
        displayId: string,
        machineId: string,
        screenGeometry: Rect,
        preferredPosition?: PreferredPosition
    ): void {
        if (preferredPosition === undefined) {
            preferredPosition = { side: "right" };
        }
        this.#connectedInstances.set(instanceId, {
            displayId,
            machineId,
            lastSeen: Date.now(),
            screenGeometry,
            preferredPosition
        });

        // Clean up old instances (older than 30 seconds)
        const cutoff = Date.now() - 30000;
        for (const [id, info] of this.#connectedInstances) {
            if (info.lastSeen < cutoff) {
                this.#connectedInstances.delete(id);
            }
        }
    }

    #hasSameDisplayInstance(): boolean {
        // Check if any connected instance is on the same display
        for (const [instanceId, info] of this.#connectedInstances) {
            if (
                instanceId !== this.#config.instanceId &&
                info.displayId === this.#displayId &&
                info.machineId === this.#machineId
            ) {
                return true;
            }
        }
        return false;
    }

    #shouldProcessInputEvent(): boolean {
        // Only process input events if this instance should be the active source
        const shouldProcess = this.#isActiveInstance;
        verbose(
            `shouldProcessInputEvent: isActiveInstance=${this.#isActiveInstance}, role=${this.#role === InstanceRole.Server ? "server" : "client"}`
        );
        return shouldProcess;
    }

    // Input event broadcasting methods for source/target architecture
    #broadcastInputEvent(eventType: InputEventType, eventData: InputEventData): void {
        const message: InputEventMessage = {
            type: "input_event",
            sourceInstanceId: this.#config.instanceId,
            sourceDisplayId: this.#displayId,
            sourceMachineId: this.#machineId,
            eventType,
            eventData
        };

        // Broadcast to all connected WebSocket clients (excluding console connections)
        this.#server.broadcastToClients(JSON.stringify(message));

        verbose(`Broadcasted ${eventType} event from source instance ${this.#config.instanceId}`);
    }

    #handleInputEvent(message: InputEventMessage): void {
        // Only handle input events if we're a client (servers don't execute received input events)
        if (this.#role === InstanceRole.Server) {
            return; // Servers don't execute received input events
        }

        // Don't execute events from the same instance
        if (message.sourceInstanceId === this.#config.instanceId) {
            return;
        }

        // Execute the input event on this target instance
        verbose(`Executing ${message.eventType} event from source ${message.sourceInstanceId}`);
        this.#executeInputEvent(message.eventType, message.eventData);
    }

    #executeInputEvent(eventType: InputEventType, eventData: InputEventData): void {
        try {
            if (eventType === "keyPress" || eventType === "keyRelease") {
                this.#native.sendKeyEvent({
                    type: eventType,
                    x: eventData.x,
                    y: eventData.y,
                    dx: eventData.dx ?? 0,
                    dy: eventData.dy ?? 0,
                    timestamp: eventData.timestamp,
                    keyboardModifiers: eventData.keyboardModifiers,
                    mouseButtons: eventData.mouseButtons,
                    keycode: eventData.keycode || 0,
                    text: eventData.text
                } as KonfliktKeyPressEvent | KonfliktKeyReleaseEvent);
            } else {
                this.#native.sendMouseEvent({
                    type: eventType,
                    x: eventData.x,
                    y: eventData.y,
                    dx: eventData.dx ?? 0,
                    dy: eventData.dy ?? 0,
                    timestamp: eventData.timestamp,
                    keyboardModifiers: eventData.keyboardModifiers,
                    mouseButtons: eventData.mouseButtons,
                    button: eventData.button
                } as KonfliktMouseButtonPressEvent | KonfliktMouseButtonReleaseEvent | KonfliktMouseMoveEvent);

                // After executing a mouse move, check if cursor is at left edge trying to go further left
                // The client knows the actual cursor position - use it to decide when to transition back
                // Only clients need to do this check (role check instead of isActiveInstance flag)
                if (eventType === "mouseMove" && this.#role === InstanceRole.Client) {
                    const actualState = this.#native.state;
                    const dx = eventData.dx ?? 0;

                    verbose(`Client mouse move: actualX=${actualState.x}, dx=${dx}`);

                    // If cursor is at left edge (x <= 1) and user is moving left, request deactivation
                    if (actualState.x <= 1 && dx < 0) {
                        verbose(`Client at left edge (x=${actualState.x}) with dx=${dx}, requesting deactivation`);
                        this.#requestDeactivation();
                    }
                }
            }
        } catch (err) {
            verbose(`Failed to execute ${eventType} event:`, err);
        }
    }

    #requestDeactivation(): void {
        if (!this.#peerManager) {
            return;
        }

        // Debounce: only send one request per 500ms
        const now = Date.now();
        if (now - this.#lastDeactivationRequest < 500) {
            return;
        }
        this.#lastDeactivationRequest = now;

        const message = {
            type: "deactivation_request",
            instanceId: this.#config.instanceId,
            timestamp: now
        };

        this.#peerManager.broadcast(JSON.stringify(message));
        log(`Sent deactivation request to server`);
    }

    #sendInstanceInfo(): void {
        const message: InstanceInfoMessage = {
            type: "instance_info",
            instanceId: this.#config.instanceId,
            displayId: this.#displayId,
            machineId: this.#machineId,
            timestamp: Date.now(),
            screenGeometry: this.#screenBounds
        };

        this.#server.broadcastToClients(JSON.stringify(message));
        debug(`Sent instance info from ${this.#config.instanceId}`);
    }

    #handleInstanceInfo(message: InstanceInfoMessage): void {
        // Update our tracking of connected instances
        this.#updateConnectedInstance(message.instanceId, message.displayId, message.machineId, message.screenGeometry);
        verbose(`Received instance info from ${message.instanceId}`);
    }

    #onDesktopChanged(event: KonfliktDesktopEvent): void {
        verbose("Desktop changed:", event);

        // Update screen bounds if dimensions weren't manually configured
        if (!this.#config.screenWidth || !this.#config.screenHeight) {
            this.#screenBounds = new Rect(
                this.#config.screenX,
                this.#config.screenY,
                this.#config.screenWidth ?? event.desktop.width,
                this.#config.screenHeight ?? event.desktop.height
            );
            verbose("Updated screen bounds:", this.#screenBounds);
        }
    }

    #onKeyPress(event: KonfliktKeyPressEvent): void {
        // Update cursor position from event
        this.#updateCursorPosition(event.x, event.y);

        // Use virtual cursor position if remote screen is active
        const cursorPos = this.#virtualCursorPosition ?? { x: event.x, y: event.y };

        if (!this.#shouldProcessInputEvent() && this.#virtualCursorPosition === null) {
            return;
        }

        verbose("Key pressed (active source):", event);
        this.#broadcastInputEvent("keyPress", {
            x: cursorPos.x,
            y: cursorPos.y,
            timestamp: event.timestamp,
            keyboardModifiers: event.keyboardModifiers,
            mouseButtons: event.mouseButtons,
            keycode: event.keycode,
            text: event.text
        });
    }

    #onKeyRelease(event: KonfliktKeyReleaseEvent): void {
        // Update cursor position from event
        this.#updateCursorPosition(event.x, event.y);

        // Use virtual cursor position if remote screen is active
        const cursorPos = this.#virtualCursorPosition ?? { x: event.x, y: event.y };

        if (!this.#shouldProcessInputEvent() && this.#virtualCursorPosition === null) {
            return;
        }

        verbose("Key released (active source):", event);
        this.#broadcastInputEvent("keyRelease", {
            x: cursorPos.x,
            y: cursorPos.y,
            timestamp: event.timestamp,
            keyboardModifiers: event.keyboardModifiers,
            mouseButtons: event.mouseButtons,
            keycode: event.keycode,
            text: event.text
        });
    }

    #onMousePress(event: KonfliktMouseButtonPressEvent): void {
        // Update cursor position from event
        this.#updateCursorPosition(event.x, event.y);

        // Use virtual cursor position if remote screen is active
        const cursorPos = this.#virtualCursorPosition ?? { x: event.x, y: event.y };

        if (!this.#shouldProcessInputEvent() && this.#virtualCursorPosition === null) {
            return;
        }

        verbose("Mouse button pressed (active source):", event);
        this.#broadcastInputEvent("mousePress", {
            x: cursorPos.x,
            y: cursorPos.y,
            timestamp: event.timestamp,
            keyboardModifiers: event.keyboardModifiers,
            mouseButtons: event.mouseButtons,
            button: event.button
        });
    }

    #onMouseRelease(event: KonfliktMouseButtonReleaseEvent): void {
        // Update cursor position from event
        this.#updateCursorPosition(event.x, event.y);

        // Use virtual cursor position if remote screen is active
        const cursorPos = this.#virtualCursorPosition ?? { x: event.x, y: event.y };

        if (!this.#shouldProcessInputEvent() && this.#virtualCursorPosition === null) {
            return;
        }

        verbose("Mouse button released (active source):", event);
        this.#broadcastInputEvent("mouseRelease", {
            x: cursorPos.x,
            y: cursorPos.y,
            timestamp: event.timestamp,
            keyboardModifiers: event.keyboardModifiers,
            mouseButtons: event.mouseButtons,
            button: event.button
        });
    }

    #onMouseMove(event: KonfliktMouseMoveEvent): void {
        // Update cursor position from event - this is the primary cursor tracking mechanism
        this.#updateCursorPosition(event.x, event.y);

        // Check if a remote screen is active (virtual cursor mode)
        if (this.#virtualCursorPosition !== null && this.#activeRemoteScreenBounds !== null) {
            // Update virtual cursor position using deltas
            const newX = this.#virtualCursorPosition.x + event.dx;
            const newY = this.#virtualCursorPosition.y + event.dy;

            // Clamp to remote screen bounds
            // The client will send a deactivation_request when it detects
            // that the cursor is at the left edge and user is moving left
            this.#virtualCursorPosition = {
                x: Math.max(0, Math.min(this.#activeRemoteScreenBounds.width - 1, newX)),
                y: Math.max(0, Math.min(this.#activeRemoteScreenBounds.height - 1, newY))
            };

            verbose(`Virtual cursor moved to (${this.#virtualCursorPosition.x}, ${this.#virtualCursorPosition.y})`);

            // Broadcast the input event with virtual cursor position
            this.#broadcastInputEvent("mouseMove", {
                x: this.#virtualCursorPosition.x,
                y: this.#virtualCursorPosition.y,
                dx: event.dx,
                dy: event.dy,
                timestamp: event.timestamp,
                keyboardModifiers: event.keyboardModifiers,
                mouseButtons: event.mouseButtons
            });
            return;
        }

        if (!this.#shouldProcessInputEvent()) {
            return;
        }

        // Check if cursor should transition to another screen
        if (this.#checkScreenTransition(event.x, event.y)) {
            return; // Transition handled, don't broadcast
        }

        verbose("Mouse moved (active source):", event);
        this.#broadcastInputEvent("mouseMove", {
            x: event.x,
            y: event.y,
            dx: event.dx,
            dy: event.dy,
            timestamp: event.timestamp,
            keyboardModifiers: event.keyboardModifiers,
            mouseButtons: event.mouseButtons
        });
    }

    // Client connection management methods
    #setupPeerConnectionHandlers(): void {
        if (!this.#peerManager) {
            return;
        }

        this.#peerManager.on("peer_connected", (service: DiscoveredService, capabilities: string[]) => {
            verbose(`Connected to server ${service.name} at ${service.host}:${service.port}`);
            verbose(`Server capabilities: ${capabilities.join(", ")}`);

            // Send handshake with client information
            this.#sendClientHandshake(service);
        });

        this.#peerManager.on("peer_disconnected", (service: DiscoveredService, reason?: string) => {
            verbose(`Disconnected from server ${service.name}: ${reason || "unknown reason"}`);
        });

        this.#peerManager.on(
            "input_event",
            (
                event: MouseMoveEvent | MousePressEvent | MouseReleaseEvent | KeyPressEvent | KeyReleaseEvent,
                from: DiscoveredService
            ) => {
                verbose(`Received input event from server ${from.name}:`, event);
                // TODO: Execute the input event locally
            }
        );

        this.#peerManager.on("message", (message: Message, from: DiscoveredService) => {
            verbose(`Received message from server ${from.name}: ${message.type}`);
            // Cast to NetworkMessage - the types that PeerManager emits are all NetworkMessage compatible
            this.handleNetworkMessage(message as NetworkMessage);
        });

        this.#peerManager.on("error", (err: Error, service?: DiscoveredService) => {
            verbose(`Peer connection error${service ? ` with ${service.name}` : ""}: ${err.message}`);
        });

        this.#peerManager.on("update_required", (serverCommit: string, clientCommit: string) => {
            log(`Server requires update: server at ${serverCommit}, client at ${clientCommit}`);
            log(`Sending restart request to server, then exiting with code 42`);

            // Tell server to restart too so both sides update together
            const restartMessage = {
                type: "restart_request",
                reason: "version_mismatch",
                clientCommit,
                serverCommit,
                timestamp: Date.now()
            };
            this.#peerManager?.broadcast(JSON.stringify(restartMessage));

            // Give the message time to be sent before exiting
            setTimeout(() => {
                process.exit(42);
            }, 100);
        });
    }

    #connectToServer(service: DiscoveredService): void {
        if (!this.#peerManager) {
            return;
        }

        log(`Attempting to connect to server ${service.name} at ${service.host}:${service.port}`);
        this.#peerManager.connectToPeer(service, this.#screenBounds).catch((err: Error) => {
            error(`Failed to connect to server ${service.name}: ${err.message}`);
        });
    }

    #sendClientHandshake(service: DiscoveredService): void {
        // The handshake is already handled by the PeerManager/WebSocketClient
        // Now send the ClientRegistration message with screen info
        verbose(`Client ${this.#config.instanceId} handshake completed with server ${service.name}`);
        verbose(`Client screen geometry: ${JSON.stringify(this.#screenBounds)}`);

        // Send ClientRegistration message to server
        const registration: ClientRegistrationMessage = {
            type: "client_registration",
            instanceId: this.#config.instanceId,
            displayName: this.#config.instanceName,
            machineId: this.#machineId,
            screenWidth: this.#screenBounds.width,
            screenHeight: this.#screenBounds.height
        };

        if (this.#peerManager) {
            this.#peerManager.broadcast(JSON.stringify(registration));
        }
    }

    // Server quit request method
    static async #requestServerQuit(service: DiscoveredService): Promise<void> {
        try {
            const WebSocket = (await import("ws")).default;
            const wsUrl = `ws://${service.host}:${service.port}/console`;

            verbose(`Connecting to existing server console at ${wsUrl} to request quit...`);
            const ws = new WebSocket(wsUrl);

            await new Promise<void>((resolve: () => void, reject: (err: Error) => void) => {
                const timeout = setTimeout(() => {
                    ws.close();
                    reject(new Error("Timeout connecting to existing server"));
                }, 5000);

                ws.on("open", () => {
                    clearTimeout(timeout);
                    const quitMessage = {
                        type: "console_command",
                        command: "quit",
                        args: [],
                        timestamp: Date.now()
                    };

                    ws.send(JSON.stringify(quitMessage));
                    verbose("Sent quit command to existing server");

                    // Give the server a moment to process the quit
                    setTimeout(() => {
                        ws.close();
                        resolve();
                    }, 1000);
                });

                ws.on("error", (err: Error) => {
                    clearTimeout(timeout);
                    reject(err);
                });
            });
        } catch (err) {
            error(`Failed to request server quit: ${err}`);
            log("Existing server may already be shutting down or unreachable");
        }
    }

    // Same-display detection methods
    static #generateMachineId(): string {
        // Create a unique but consistent identifier for this machine
        const machineInfo = `${hostname()}-${platform()}-${process.env.USER || process.env.USERNAME || "unknown"}`;
        return createHash("sha256").update(machineInfo).digest("hex").substring(0, 16);
    }
}
