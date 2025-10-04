import { Console } from "./Console.js";
import { KonfliktNative as KonfliktNativeConstructor } from "./native.js";
import { Server } from "./Server.js";
import { createHash } from "crypto";
import { createNativeLogger, setLogBroadcaster, verbose } from "./Log";
import { hostname, platform } from "os";
import type { Config } from "./Config.js";
import type { InputEventMessage, InstanceInfoMessage } from "./messageValidation";
import type {
    KonfliktDesktopEvent,
    KonfliktKeyPressEvent,
    KonfliktKeyReleaseEvent,
    KonfliktMouseButtonPressEvent,
    KonfliktMouseButtonReleaseEvent,
    KonfliktMouseMoveEvent
} from "./KonfliktNative";
import type { KonfliktNative } from "./KonfliktNative.js";

export interface Rect {
    x: number;
    y: number;
    width: number;
    height: number;
}

export interface ConnectedInstanceInfo {
    displayId: string;
    machineId: string;
    lastSeen: number;
}

export class Konflikt {
    #config: Config;
    #native: KonfliktNative;
    #server: Server;
    #console: Console | undefined;

    // Active instance management
    #currentMode: "source" | "target" | "auto";
    #isActiveInstance: boolean = false;
    #lastCursorPosition: { x: number; y: number } = { x: 0, y: 0 };
    #screenBounds: Rect;

    // Same-display detection for loop prevention
    #displayId: string;
    #machineId: string;
    #connectedInstances: Map<string, ConnectedInstanceInfo> = new Map();

    constructor(config: Config) {
        this.#config = config;
        this.#native = new KonfliktNativeConstructor(createNativeLogger());
        this.#server = new Server(config.port, config.instanceId, config.instanceName);

        this.#currentMode = config.mode;

        // Generate machine and display identifiers for loop detection
        this.#machineId = this.#generateMachineId();
        this.#displayId = this.#generateDisplayId();

        // Calculate screen bounds from config
        const desktop = this.#native.desktop;
        this.#screenBounds = {
            x: config.screenX,
            y: config.screenY,
            width: config.screenWidth ?? desktop.width,
            height: config.screenHeight ?? desktop.height
        };

        // Console will be created during init if stdin is available
        this.#native.on("keyPress", this.#onKeyPress.bind(this));
        this.#native.on("keyRelease", this.#onKeyRelease.bind(this));
        this.#native.on("mousePress", this.#onMousePress.bind(this));
        this.#native.on("mouseRelease", this.#onMouseRelease.bind(this));
        this.#native.on("mouseMove", this.#onMouseMove.bind(this));
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

    get currentMode(): "source" | "target" | "auto" {
        return this.#currentMode;
    }

    get isActiveInstance(): boolean {
        return this.#isActiveInstance;
    }

    get screenBounds(): Rect {
        return { ...this.#screenBounds };
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
        verbose("Initializing Konflikt...", this.#config);

        // Pass config to server for console commands
        this.#server.setConfig(this.#config);

        // Set up message handler for input events
        this.#server.setMessageHandler(this.handleNetworkMessage.bind(this));

        await this.#server.start();

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

    // Active instance management methods
    #updateCursorPosition(x: number, y: number): void {
        this.#lastCursorPosition = { x, y };
        this.#checkIfShouldBeActive();
    }

    #checkIfShouldBeActive(): void {
        const wasPreviouslyActive = this.#isActiveInstance;

        if (this.#currentMode === "source") {
            this.#isActiveInstance = true;
        } else if (this.#currentMode === "target") {
            this.#isActiveInstance = false;
        } else if (this.#currentMode === "auto") {
            // Check if cursor is within this instance's screen bounds
            this.#isActiveInstance = this.#isCursorInScreenBounds(this.#lastCursorPosition);
        }

        // Log state changes
        if (wasPreviouslyActive !== this.#isActiveInstance) {
            verbose(
                `Instance ${this.#config.instanceId} ${this.#isActiveInstance ? "became ACTIVE" : "became INACTIVE"} (mode: ${this.#currentMode}, cursor: ${this.#lastCursorPosition.x}, ${this.#lastCursorPosition.y})`
            );
        }
    }

    #isCursorInScreenBounds(position: { x: number; y: number }): boolean {
        return Konflikt.pointInRect(position, this.#screenBounds);
    }

    static pointInRect(point: { x: number; y: number }, rect: Rect): boolean {
        return (
            point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y && point.y < rect.y + rect.height
        );
    }

    // Same-display detection methods
    #generateMachineId(): string {
        // Create a unique but consistent identifier for this machine
        const machineInfo = `${hostname()}-${platform()}-${process.env.USER || process.env.USERNAME || "unknown"}`;
        return createHash("sha256").update(machineInfo).digest("hex").substring(0, 16);
    }

    #generateDisplayId(): string {
        // Create a display identifier based on desktop dimensions and position
        const desktop = this.#native.desktop;
        const displayInfo = `${desktop.width}x${desktop.height}-${this.#screenBounds.x},${this.#screenBounds.y}`;
        return createHash("sha256").update(`${this.#machineId}-${displayInfo}`).digest("hex").substring(0, 16);
    }

    #updateConnectedInstance(instanceId: string, displayId: string, machineId: string): void {
        this.#connectedInstances.set(instanceId, {
            displayId,
            machineId,
            lastSeen: Date.now()
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

    #shouldPreventEventLoop(): boolean {
        // Prevent processing events if we're in auto mode and there's another instance on the same display
        const shouldPrevent = this.#currentMode === "auto" && this.#hasSameDisplayInstance();

        if (shouldPrevent) {
            verbose(
                `Event loop prevention: Instance ${this.#config.instanceId} detected same-display instance, suppressing input`
            );
        }

        return shouldPrevent;
    }

    #shouldProcessInputEvent(): boolean {
        // Prevent event loops first
        if (this.#shouldPreventEventLoop()) {
            return false;
        }

        // Only process input events if this instance should be the active source
        return this.#isActiveInstance || this.#currentMode === "source";
    }

    // Input event broadcasting methods for source/target architecture
    #broadcastInputEvent(
        eventType: "keyPress" | "keyRelease" | "mousePress" | "mouseRelease" | "mouseMove", 
        eventData: {
            x: number;
            y: number;
            timestamp: number;
            keyboardModifiers: number;
            mouseButtons: number;
            keycode?: number;
            text?: string;
            button?: number;
        }
    ): void {
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
        // Only handle input events if we're in target mode or auto mode and not the active instance
        if (this.#currentMode === "source") {
            return; // Source instances don't execute received input events
        }

        // Update connected instance info for same-display detection
        this.#updateConnectedInstance(
            message.sourceInstanceId,
            message.sourceDisplayId,
            message.sourceMachineId
        );

        // Don't execute events from the same instance
        if (message.sourceInstanceId === this.#config.instanceId) {
            return;
        }

        // Execute the input event on this target instance
        verbose(`Executing ${message.eventType} event from source ${message.sourceInstanceId}`);
        this.#executeInputEvent(message.eventType, message.eventData);
    }

    #executeInputEvent(
        eventType: "keyPress" | "keyRelease" | "mousePress" | "mouseRelease" | "mouseMove",
        eventData: {
            x: number;
            y: number;
            timestamp: number;
            keyboardModifiers: number;
            mouseButtons: number;
            keycode?: number;
            text?: string;
            button?: number;
        }
    ): void {
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
            timestamp: Date.now()
        };

        this.#server.broadcastToClients(JSON.stringify(message));
        verbose(`Sent instance info from ${this.#config.instanceId}`);
    }

    #handleInstanceInfo(message: InstanceInfoMessage): void {
        // Update our tracking of connected instances
        this.#updateConnectedInstance(message.instanceId, message.displayId, message.machineId);
        verbose(`Received instance info from ${message.instanceId}`);
    }

    // Method to be called by Server when receiving network messages
    handleNetworkMessage(message: InputEventMessage | InstanceInfoMessage): void {
        if (message.type === "input_event") {
            this.#handleInputEvent(message);
        } else if (message.type === "instance_info") {
            this.#handleInstanceInfo(message);
        }
    }

    #onDesktopChanged(event: KonfliktDesktopEvent): void {
        verbose("Desktop changed:", event);

        // Update screen bounds if dimensions weren't manually configured
        if (!this.#config.screenWidth || !this.#config.screenHeight) {
            this.#screenBounds = {
                x: this.#config.screenX,
                y: this.#config.screenY,
                width: this.#config.screenWidth ?? event.desktop.width,
                height: this.#config.screenHeight ?? event.desktop.height
            };
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
}
