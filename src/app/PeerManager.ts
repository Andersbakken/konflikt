import { EventEmitter } from "events";
import { WebSocketClient } from "./WebSocketClient";
import { createBaseMessage } from "./createBaseMessage";
import { debug } from "./debug";
import { error } from "./error";
import { log } from "./log";
import { verbose } from "./verbose";
import type { DiscoveredService } from "./DiscoveredService";
import type { HandshakeResponse } from "./HandshakeResponse";
import type { KeyPressEvent } from "./KeyPressEvent";
import type { KeyReleaseEvent } from "./KeyReleaseEvent";
import type { Message } from "./Message";
import type { MouseMoveEvent } from "./MouseMoveEvent";
import type { MousePressEvent } from "./MousePressEvent";
import type { MouseReleaseEvent } from "./MouseReleaseEvent";
import type { PendingReconnect } from "./PendingReconnect";
import type { Rect } from "./Rect";

interface PeerManagerEvents {
    peer_connected: [service: DiscoveredService, capabilities: string[]];
    peer_disconnected: [service: DiscoveredService, reason?: string];
    input_event: [
        event: MouseMoveEvent | MousePressEvent | MouseReleaseEvent | KeyPressEvent | KeyReleaseEvent,
        from: DiscoveredService
    ];
    message: [message: Message, from: DiscoveredService];
    error: [error: Error, service?: DiscoveredService];
}

export class PeerManager extends EventEmitter<PeerManagerEvents> {
    #clients = new Map<string, WebSocketClient>();
    #pendingReconnects = new Map<string, PendingReconnect>();
    #reconnectTimers = new Map<string, NodeJS.Timeout>();
    #instanceId: string;
    #instanceName: string;
    #version: string;
    #capabilities: string[];
    #isDestroyed = false;

    constructor(
        instanceId: string,
        instanceName: string,
        version: string = "1.0.0",
        capabilities: string[] = ["input_events", "state_sync"]
    ) {
        super();
        this.#instanceId = instanceId;
        this.#instanceName = instanceName;
        this.#version = version;
        this.#capabilities = capabilities;
    }

    /**
     * Connect to a discovered peer service
     */
    async connectToPeer(service: DiscoveredService, screenGeometry?: Rect): Promise<void> {
        const serviceKey = `${service.host}:${service.port}`;

        // Don't connect to ourselves
        if (service.pid === process.pid) {
            debug(`Skipping connection to self: ${service.name}`);
            return;
        }

        // Don't connect if already connected
        if (this.#clients.has(serviceKey)) {
            debug(`Already connected to peer: ${service.name}`);
            return;
        }

        debug(`Connecting to peer: ${service.name} at ${serviceKey}`);

        const client = new WebSocketClient(
            service,
            this.#instanceId,
            this.#instanceName,
            this.#version,
            screenGeometry,
            this.#capabilities
        );

        // Set up event handlers
        client.on("connected", (connectedService: DiscoveredService) => {
            log(`Connected to peer: ${connectedService.name}`);
        });

        client.on("handshake_completed", (connectedService: DiscoveredService, response: HandshakeResponse) => {
            log(`Handshake completed with peer: ${connectedService.name} (${response.instanceName})`);
            this.emit("peer_connected", connectedService, response.capabilities);
        });

        client.on("handshake_failed", (connectedService: DiscoveredService, reason: string) => {
            error(`Handshake failed with peer ${connectedService.name}: ${reason}`);
            this.#clients.delete(serviceKey);
        });

        client.on("disconnected", (connectedService: DiscoveredService) => {
            log(`Disconnected from peer: ${connectedService.name}`);
            this.#clients.delete(serviceKey);
            this.emit("peer_disconnected", connectedService);

            // Schedule reconnection if not destroyed
            if (!this.#isDestroyed) {
                this.#scheduleReconnect(serviceKey, service, screenGeometry);
            }
        });

        client.on("message", (message: Message, from: DiscoveredService) => {
            this.#handlePeerMessage(message, from);
        });

        client.on("error", (err: Error, connectedService: DiscoveredService) => {
            error(`Error with peer ${connectedService.name}:`, err);
            this.emit("error", err, connectedService);
        });

        this.#clients.set(serviceKey, client);

        try {
            await client.connect();
        } catch (err) {
            error(`Failed to connect to peer ${service.name}:`, err);
            this.#clients.delete(serviceKey);
            throw err;
        }
    }

    /**
     * Disconnect from a peer
     */
    disconnectFromPeer(service: DiscoveredService, reason?: string): void {
        const serviceKey = `${service.host}:${service.port}`;
        const client = this.#clients.get(serviceKey);

        if (client) {
            client.disconnect(reason);
            this.#clients.delete(serviceKey);
        }
    }

    /**
     * Disconnect from all peers
     */
    disconnectAll(reason?: string): void {
        debug(`Disconnecting from all peers: ${reason || "Shutdown"}`);
        for (const client of this.#clients.values()) {
            client.disconnect(reason);
        }
        this.#clients.clear();
    }

    /**
     * Broadcast a raw message to all connected peers
     */
    broadcast(message: string): void {
        let sentCount = 0;
        for (const client of this.#clients.values()) {
            if (client.isConnected && client.isHandshakeComplete) {
                if (client.sendRaw(message)) {
                    sentCount++;
                }
            }
        }
        verbose(`Broadcasted message to ${sentCount} peers`);
    }

    /**
     * Broadcast an input event to all connected peers
     */
    broadcastInputEvent(
        event: MouseMoveEvent | MousePressEvent | MouseReleaseEvent | KeyPressEvent | KeyReleaseEvent
    ): void {
        const eventWithMetadata = {
            ...event,
            ...createBaseMessage(this.#instanceId)
        };

        let sentCount = 0;
        for (const client of this.#clients.values()) {
            if (client.isConnected && client.isHandshakeComplete) {
                if (client.send(eventWithMetadata)) {
                    sentCount++;
                }
            }
        }

        verbose(`Broadcasted ${event.type} to ${sentCount} peers`);
    }

    /**
     * Send a message to a specific peer
     */
    sendToPeer(service: DiscoveredService, message: Message): boolean {
        const serviceKey = `${service.host}:${service.port}`;
        const client = this.#clients.get(serviceKey);

        if (client && client.isConnected && client.isHandshakeComplete) {
            return client.send(message);
        }

        return false;
    }

    /**
     * Get list of connected peers
     */
    getConnectedPeers(): DiscoveredService[] {
        return Array.from(this.#clients.values())
            .filter((client: WebSocketClient) => client.isConnected && client.isHandshakeComplete)
            .map((client: WebSocketClient) => client.connectedService);
    }

    /**
     * Get connection statistics
     */
    getStats(): {
        totalConnections: number;
        activeConnections: number;
        connectedPeers: string[];
    } {
        const connectedPeers = this.getConnectedPeers();
        return {
            totalConnections: this.#clients.size,
            activeConnections: connectedPeers.length,
            connectedPeers: connectedPeers.map((peer: DiscoveredService) => peer.name)
        };
    }

    /**
     * Handle incoming messages from peers
     */
    #handlePeerMessage(message: Message, from: DiscoveredService): void {
        verbose(`Received message from ${from.name}:`, message.type);

        // Handle input event messages
        if (
            message.type === "mouse_move" ||
            message.type === "mouse_press" ||
            message.type === "mouse_release" ||
            message.type === "key_press" ||
            message.type === "key_release"
        ) {
            this.emit(
                "input_event",
                message as MouseMoveEvent | MousePressEvent | MouseReleaseEvent | KeyPressEvent | KeyReleaseEvent,
                from
            );
            return;
        }

        // Handle other message types as needed
        switch (message.type) {
            case "heartbeat":
                // Heartbeats are handled automatically by WebSocketClient
                break;

            case "disconnect":
                debug(`Peer ${from.name} is disconnecting: ${message.reason || "No reason given"}`);
                break;

            case "error":
                error(`Error message from peer ${from.name}:`, message.message);
                break;

            case "handshake_request":
            case "handshake_response":
                // These are handled by WebSocketClient
                break;

            case "input_event":
                // Forward to application layer for execution
                verbose(`Received wrapped input_event: ${message.eventType}`);
                this.emit("message", message, from);
                break;

            case "layout_update":
            case "layout_assignment":
            case "activate_client":
            case "client_registration":
            case "instance_info":
                // Forward to application layer (Konflikt.ts)
                verbose(`Received ${message.type} message from ${from.name}`);
                this.emit("message", message, from);
                break;

            default:
                // Treat unknown message types as errors
                error(`Unknown message type from ${from.name}:`, JSON.stringify(message, null, 4));
        }
    }

    /**
     * Schedule a reconnection attempt with exponential backoff
     */
    #scheduleReconnect(serviceKey: string, service: DiscoveredService, screenGeometry?: Rect): void {
        // Get or initialize reconnect info
        const existing = this.#pendingReconnects.get(serviceKey);
        const reconnectInfo = existing ?? { service, screenGeometry, attempts: 0 };
        if (!existing) {
            this.#pendingReconnects.set(serviceKey, reconnectInfo);
        }
        reconnectInfo.attempts++;

        // Calculate delay with exponential backoff (1s, 2s, 4s, 8s, max 30s)
        const delay = Math.min(1000 * Math.pow(2, reconnectInfo.attempts - 1), 30000);

        log(`Scheduling reconnect to ${service.name} in ${delay / 1000}s (attempt ${reconnectInfo.attempts})`);

        // Clear any existing timer
        const existingTimer = this.#reconnectTimers.get(serviceKey);
        if (existingTimer) {
            clearTimeout(existingTimer);
        }

        // Schedule reconnection
        const timer = setTimeout(() => {
            this.#reconnectTimers.delete(serviceKey);

            if (this.#isDestroyed) {
                return;
            }

            // Don't reconnect if already connected
            if (this.#clients.has(serviceKey)) {
                this.#pendingReconnects.delete(serviceKey);
                return;
            }

            log(`Attempting reconnect to ${service.name}...`);

            this.connectToPeer(service, screenGeometry)
                .then(() => {
                    // Clear reconnect info on success
                    this.#pendingReconnects.delete(serviceKey);
                    log(`Reconnected to ${service.name}`);
                })
                .catch((err: Error) => {
                    verbose(`Reconnect to ${service.name} failed: ${err.message}`);
                    // Schedule another reconnect attempt
                    if (!this.#isDestroyed) {
                        this.#scheduleReconnect(serviceKey, service, screenGeometry);
                    }
                });
        }, delay);

        this.#reconnectTimers.set(serviceKey, timer);
    }

    /**
     * Destroy the peer manager and clean up all connections
     */
    destroy(): void {
        this.#isDestroyed = true;

        // Clear all reconnect timers
        for (const timer of this.#reconnectTimers.values()) {
            clearTimeout(timer);
        }
        this.#reconnectTimers.clear();
        this.#pendingReconnects.clear();

        // Disconnect all clients
        for (const [, client] of this.#clients) {
            client.destroy();
        }
        this.#clients.clear();
    }
}
