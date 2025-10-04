import { EventEmitter } from "events";
import { createBaseMessage, createErrorMessage, validateMessage } from "./messages";
import { debug, error, log, verbose } from "./Log";
import WebSocket from "ws";
import type { DiscoveredService } from "./DiscoveredService";
import type { HandshakeRequest, HandshakeResponse, Message } from "./messages";

export interface WebSocketClientEvents {
    connected: [service: DiscoveredService];
    disconnected: [service: DiscoveredService, reason?: string];
    handshake_completed: [service: DiscoveredService, response: HandshakeResponse];
    handshake_failed: [service: DiscoveredService, reason: string];
    message: [message: Message, service: DiscoveredService];
    error: [error: Error, service: DiscoveredService];
}

export class WebSocketClient extends EventEmitter<WebSocketClientEvents> {
    #ws: WebSocket | null = null;
    #service: DiscoveredService;
    #instanceId: string;
    #instanceName: string;
    #version: string;
    #capabilities: string[];
    #heartbeatInterval: NodeJS.Timeout | null = null;
    #reconnectTimeout: NodeJS.Timeout | null = null;
    #isHandshakeCompleted = false;
    #isConnecting = false;
    #isDestroyed = false;

    constructor(
        service: DiscoveredService,
        instanceId: string,
        instanceName: string,
        version: string,
        capabilities: string[] = []
    ) {
        super();
        this.#service = service;
        this.#instanceId = instanceId;
        this.#instanceName = instanceName;
        this.#version = version;
        this.#capabilities = capabilities;
    }

    get isConnected(): boolean {
        return this.#ws?.readyState === WebSocket.OPEN;
    }

    get isHandshakeComplete(): boolean {
        return this.#isHandshakeCompleted;
    }

    get connectedService(): DiscoveredService {
        return this.#service;
    }

    async connect(): Promise<void> {
        if (this.#isConnecting || this.isConnected || this.#isDestroyed) {
            return;
        }

        this.#isConnecting = true;

        try {
            const url = `ws://${this.#service.host}:${this.#service.port}/ws`;
            debug(`Connecting to peer at ${url}`);

            this.#ws = new WebSocket(url);

            this.#ws.on("open", this.#onOpen.bind(this));
            this.#ws.on("message", this.#onMessage.bind(this));
            this.#ws.on("close", this.#onClose.bind(this));
            this.#ws.on("error", this.#onError.bind(this));

            // Set up connection timeout
            const connectTimeout = setTimeout(() => {
                if (this.#isConnecting) {
                    this.#ws?.terminate();
                    this.#onError(new Error("Connection timeout"));
                }
            }, 10000); // 10 second timeout

            this.#ws.on("open", () => {
                clearTimeout(connectTimeout);
            });
        } catch (err) {
            this.#isConnecting = false;
            this.#onError(err as Error);
        }
    }

    disconnect(reason?: string): void {
        if (this.#isDestroyed) {
            return;
        }

        debug(`Disconnecting from peer ${this.#service.name}: ${reason || "Manual disconnect"}`);

        // Send disconnect message if connected
        if (this.isConnected && this.#isHandshakeCompleted) {
            this.send({
                ...createBaseMessage(this.#instanceId),
                type: "disconnect",
                reason
            });
        }

        this.#cleanup();
    }

    destroy(): void {
        this.#isDestroyed = true;
        this.disconnect("Client destroyed");
    }

    send(message: Message): boolean {
        if (!this.isConnected || this.#isDestroyed) {
            debug(`Cannot send message - not connected to ${this.#service.name}`);
            return false;
        }

        try {
            this.#ws!.send(JSON.stringify(message));
            verbose(`Sent message to ${this.#service.name}:`, message.type);
            return true;
        } catch (err) {
            error(`Failed to send message to ${this.#service.name}:`, err);
            return false;
        }
    }

    #onOpen(): void {
        this.#isConnecting = false;
        debug(`Connected to peer ${this.#service.name}`);

        this.emit("connected", this.#service);
        this.#startHandshake();
    }

    #onMessage(text: WebSocket.RawData): void {
        try {
            // We only expect string messages
            if (typeof text !== "string") {
                this.#sendError("INVALID_MESSAGE_FORMAT", "Only string messages are supported");
                return;
            }

            let rawMessage: unknown;
            try {
                rawMessage = JSON.parse(text);
            } catch {
                this.#sendError("INVALID_JSON", "Message is not valid JSON");
                return;
            }
            const message = validateMessage(rawMessage);

            if (!message) {
                error(`Invalid message format from ${this.#service.name}:`, JSON.stringify(rawMessage));
                this.#sendError("INVALID_MESSAGE", "Message validation failed");
                return;
            }

            verbose(`Received message from ${this.#service.name}:`, message.type);

            // Handle handshake messages specially
            if (message.type === "handshake_response") {
                this.#handleHandshakeResponse(message);
                return;
            }

            if (message.type === "handshake_request") {
                // We initiated the connection, so we shouldn't receive handshake requests
                this.#sendError("PROTOCOL_ERROR", "Unexpected handshake request");
                return;
            }

            // For other messages, ensure handshake is complete
            if (!this.#isHandshakeCompleted && message.type !== "error" && message.type !== "disconnect") {
                this.#sendError("HANDSHAKE_REQUIRED", "Handshake must be completed first");
                return;
            }

            // Handle special control messages
            switch (message.type) {
                case "heartbeat":
                    // Respond to heartbeat
                    this.send({
                        ...createBaseMessage(this.#instanceId),
                        type: "heartbeat"
                    });
                    break;

                case "disconnect":
                    debug(`Peer ${this.#service.name} disconnected: ${message.reason || "No reason given"}`);
                    this.#cleanup();
                    return;

                case "error":
                    error(`Error from peer ${this.#service.name}:`, message.message);
                    break;

                case "mouse_move":
                case "mouse_press":
                case "mouse_release":
                case "key_press":
                case "key_release":
                    // These will be handled by the message emit below
                    break;

                default:
                    error(`Unknown message type from ${this.#service.name}:`, message);
                    this.#sendError("UNKNOWN_MESSAGE_TYPE", "Unknown message type", message);
                    break;
            }

            // Emit the message for application handling
            this.emit("message", message, this.#service);
        } catch (err) {
            error(`Failed to parse message from ${this.#service.name}:`, err);
            this.#sendError("PARSE_ERROR", `Failed to parse message: ${String(err)}`);
        }
    }

    #onClose(code: number, reason: Buffer): void {
        debug(`Disconnected from peer ${this.#service.name} (${code}): ${reason.toString()}`);
        this.#cleanup();
    }

    #onError(err: Error): void {
        error(`WebSocket error with peer ${this.#service.name}:`, err);
        this.emit("error", err, this.#service);
        this.#cleanup();
    }

    #startHandshake(): void {
        const handshakeRequest: HandshakeRequest = {
            ...createBaseMessage(this.#instanceId),
            type: "handshake_request",
            instanceId: this.#instanceId,
            instanceName: this.#instanceName,
            version: this.#version,
            capabilities: this.#capabilities
        };

        this.send(handshakeRequest);

        // Set up handshake timeout
        setTimeout(() => {
            if (!this.#isHandshakeCompleted && this.isConnected) {
                error(`Handshake timeout with peer ${this.#service.name}`);
                this.disconnect("Handshake timeout");
            }
        }, 5000); // 5 second handshake timeout
    }

    #handleHandshakeResponse(response: HandshakeResponse): void {
        if (response.accepted) {
            this.#isHandshakeCompleted = true;
            log(`Handshake completed with peer ${this.#service.name} (${response.instanceName})`);
            this.emit("handshake_completed", this.#service, response);
            this.#startHeartbeat();
        } else {
            const reason = response.reason || "Handshake rejected";
            error(`Handshake failed with peer ${this.#service.name}: ${reason}`);
            this.emit("handshake_failed", this.#service, reason);
            this.disconnect("Handshake rejected");
        }
    }

    #startHeartbeat(): void {
        // Send heartbeat every 30 seconds
        this.#heartbeatInterval = setInterval(() => {
            if (this.isConnected && this.#isHandshakeCompleted) {
                this.send({
                    ...createBaseMessage(this.#instanceId),
                    type: "heartbeat"
                });
            }
        }, 30000);
    }

    #sendError(code: string, message: string, details?: unknown): void {
        this.send(createErrorMessage(this.#instanceId, code, message, details));
    }

    #cleanup(): void {
        if (this.#heartbeatInterval) {
            clearInterval(this.#heartbeatInterval);
            this.#heartbeatInterval = null;
        }

        if (this.#reconnectTimeout) {
            clearTimeout(this.#reconnectTimeout);
            this.#reconnectTimeout = null;
        }

        const wasConnected = this.isConnected;
        const wasHandshakeComplete = this.#isHandshakeCompleted;

        this.#isConnecting = false;
        this.#isHandshakeCompleted = false;

        if (this.#ws) {
            this.#ws.removeAllListeners();

            if (this.#ws.readyState !== WebSocket.CLOSED) {
                this.#ws.terminate();
            }

            this.#ws = null;
        }

        if (wasConnected || wasHandshakeComplete) {
            this.emit("disconnected", this.#service);
        }
    }
}
