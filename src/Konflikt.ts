import { Console } from "./Console";
import { InstanceRole } from "./InstanceRole";
import { KonfliktNative as KonfliktNativeConstructor } from "./native";
import { PeerManager } from "./PeerManager";
import { Rect } from "./Rect";
import { Server } from "./Server";
import { createHash } from "crypto";
import { createNativeLogger } from "./createNativeLogger";
import { createPromise } from "./createPromise";
import { error } from "./error";
import { hostname, platform } from "os";
import { log } from "./log";
import { setLogBroadcaster } from "./logBroadcaster";
import { verbose } from "./verbose";
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
} from "./KonfliktNative";
import type { MouseMoveEvent } from "./MouseMoveEvent";
import type { MousePressEvent } from "./MousePressEvent";
import type { MouseReleaseEvent } from "./MouseReleaseEvent";
import type { NetworkMessage } from "./NetworkMessage";
import type { Point } from "./Point";
import type { PreferredPosition } from "./PreferredPosition";
import type { PromiseData } from "./PromiseData";
import type { Side } from "./Side";

export class Konflikt {
    #config: Config;
    #native: KonfliktNative;
    #server: Server;
    #peerManager: PeerManager | undefined;
    #console: Console | undefined;

    // Role-based behavior
    #role: InstanceRole;
    #isActiveInstance: boolean = false;
    #lastCursorPosition: Point = { x: 0, y: 0 };
    #screenBounds: Rect;
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
        if (this.#role !== InstanceRole.Server) {
            return null;
        }

        // If cursor is within server's screen bounds, don't forward to clients
        if (this.#screenBounds.contains({ x, y })) {
            verbose(`Cursor at (${x}, ${y}) is within server screen bounds - not forwarding`);
            return null;
        }

        // Check adjacency configuration to find which client's screen area contains this position
        const adjacency = this.#config.adjacency;
        verbose(`Checking adjacency for cursor position (${x}, ${y}) adjacency: ${JSON.stringify(adjacency)}`);

        // TODO: Implement logic to determine target client based on screen positioning
        // This will use the adjacency configuration and connected clients' screen information

        return null; // For now, return null until we have client connection management
    }

    // Method to be called by Server when receiving network messages
    handleNetworkMessage(message: NetworkMessage): void {
        if (message.type === "input_event") {
            this.#handleInputEvent(message);
            // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        } else if (message.type !== "instance_info") {
            throw new Error(`Unknown message type: ${JSON.stringify(message)}`);
        } else {
            this.#handleInstanceInfo(message);
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
            `checkIfShouldBeActive: role=${this.#role === InstanceRole.Server ? "server" : "client"}, wasPreviouslyActive=${wasPreviouslyActive}`
        );

        if (this.#role === InstanceRole.Server) {
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
                    timestamp: eventData.timestamp,
                    keyboardModifiers: eventData.keyboardModifiers,
                    mouseButtons: eventData.mouseButtons,
                    button: eventData.button
                } as KonfliktMouseButtonPressEvent | KonfliktMouseButtonReleaseEvent | KonfliktMouseMoveEvent);
            }
        } catch (err) {
            verbose(`Failed to execute ${eventType} event:`, err);
        }
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
        verbose(`Sent instance info from ${this.#config.instanceId}`);
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

        if (!this.#shouldProcessInputEvent()) {
            return;
        }

        verbose("Key pressed (active source):", event);
        this.#broadcastInputEvent("keyPress", {
            x: event.x,
            y: event.y,
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

        if (!this.#shouldProcessInputEvent()) {
            return;
        }

        verbose("Key released (active source):", event);
        this.#broadcastInputEvent("keyRelease", {
            x: event.x,
            y: event.y,
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

        if (!this.#shouldProcessInputEvent()) {
            return;
        }

        verbose("Mouse button pressed (active source):", event);
        this.#broadcastInputEvent("mousePress", {
            x: event.x,
            y: event.y,
            timestamp: event.timestamp,
            keyboardModifiers: event.keyboardModifiers,
            mouseButtons: event.mouseButtons,
            button: event.button
        });
    }

    #onMouseRelease(event: KonfliktMouseButtonReleaseEvent): void {
        // Update cursor position from event
        this.#updateCursorPosition(event.x, event.y);

        if (!this.#shouldProcessInputEvent()) {
            return;
        }

        verbose("Mouse button released (active source):", event);
        this.#broadcastInputEvent("mouseRelease", {
            x: event.x,
            y: event.y,
            timestamp: event.timestamp,
            keyboardModifiers: event.keyboardModifiers,
            mouseButtons: event.mouseButtons,
            button: event.button
        });
    }

    #onMouseMove(event: KonfliktMouseMoveEvent): void {
        // Update cursor position from event - this is the primary cursor tracking mechanism
        this.#updateCursorPosition(event.x, event.y);

        if (!this.#shouldProcessInputEvent()) {
            return;
        }

        verbose("Mouse moved (active source):", event);
        this.#broadcastInputEvent("mouseMove", {
            x: event.x,
            y: event.y,
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

        this.#peerManager.on("error", (err: Error, service?: DiscoveredService) => {
            verbose(`Peer connection error${service ? ` with ${service.name}` : ""}: ${err.message}`);
        });
    }

    #connectToServer(service: DiscoveredService): void {
        if (!this.#peerManager) {
            return;
        }

        verbose(`Attempting to connect to server ${service.name} at ${service.host}:${service.port}`);
        this.#peerManager
            .connectToPeer(
                service,
                this.#screenBounds,
                { side: "right" as Side } // Default positioning to right side of server
            )
            .catch((err: Error) => {
                verbose(`Failed to connect to server ${service.name}: ${err.message}`);
            });
    }

    #sendClientHandshake(service: DiscoveredService): void {
        // The handshake is already handled by the PeerManager/WebSocketClient
        // But we can provide additional client-specific information via events
        verbose(`Client ${this.#config.instanceId} handshake completed with server ${service.name}`);
        verbose(`Client screen geometry: ${JSON.stringify(this.#screenBounds)}`);

        // Send additional screen positioning information to the server
        // This could be done via a separate message after handshake
        // For now, the handshake includes the screen geometry
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
