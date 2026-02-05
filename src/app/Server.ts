import { ServerConsole } from "./ServerConsole";
import { ServiceDiscovery } from "./ServiceDiscovery";
import { createBaseMessage } from "./createBaseMessage";
import { debug } from "./debug";
import { error } from "./error";
import { existsSync } from "fs";
import { getGitCommit } from "./getGitCommit";
import { isClientRegistrationMessage } from "./isClientRegistrationMessage";
import { isEADDRINUSE } from "./isEADDRINUSE";
import { isInputEventMessage } from "./isInputEventMessage";
import { isInstanceInfoMessage } from "./isInstanceInfoMessage";
import { join } from "path";
import { log } from "./log";
import { registerApiRoutes } from "./registerApiRoutes";
import { textFromWebSocketMessage } from "./textFromWebSocketMessage";
import { verbose } from "./verbose";
import Fastify from "fastify";
import WebSocket from "ws";
import fastifyStatic from "@fastify/static";
import type { ClientRegistrationMessage } from "./ClientRegistrationMessage";
import type { Config } from "./Config";
import type { DiscoveredService } from "./DiscoveredService";
import type { Duplex } from "stream";
import type { FastifyInstance, FastifyListenOptions, FastifyReply, FastifyRequest } from "fastify";
import type { HandshakeRequest } from "./HandshakeRequest";
import type { HandshakeResponse } from "./HandshakeResponse";
import type { IncomingMessage } from "http";
import type { InputEventMessage } from "./InputEventMessage";
import type { InstanceInfoMessage } from "./InstanceInfoMessage";
import type { LayoutManager } from "./LayoutManager";

export class Server {
    #fastify: FastifyInstance;
    #wss: WebSocket.WebSocketServer;
    #heartbeatInterval: ReturnType<typeof setInterval> | undefined = undefined;
    #serviceDiscovery: ServiceDiscovery;
    #instanceId: string;
    #instanceName: string;
    #version: string;
    #capabilities: string[];
    #console?: ServerConsole;
    #config?: Config;
    #port: number | null;
    #quitHandler: () => void;

    // Regular WebSocket connections (non-console)
    #regularConnections: Set<WebSocket> = new Set();

    // Map socket to client instance ID (set during handshake)
    #socketToInstanceId: Map<WebSocket, string> = new Map();

    // Message handler callback for input events and client registration
    #messageHandler?: (message: InputEventMessage | InstanceInfoMessage | ClientRegistrationMessage) => void;

    // Disconnect handler callback
    #disconnectHandler?: (instanceId: string) => void;

    // Layout manager for server-side layout management
    #layoutManager: LayoutManager | null = null;

    #role: string;
    readonly startTime: number;

    constructor(
        port: number | null,
        quitHandler: () => void,
        instanceId?: string,
        instanceName?: string,
        version: string = "1.0.0",
        capabilities: string[] = ["input_events", "state_sync"],
        role: string = "server"
    ) {
        this.#port = port;
        this.#quitHandler = quitHandler;
        this.#instanceId = instanceId || crypto.randomUUID();
        this.#instanceName = instanceName || `konflikt-${process.pid}`;
        this.#version = version;
        this.#capabilities = capabilities;
        this.#role = role;
        this.startTime = Date.now();
        // Create a custom logger that integrates with our logging system
        const customLogger = {
            level: "debug",
            stream: {
                write: (msg: string): void => {
                    // Fastify logs are JSON, try to parse and format them nicely
                    try {
                        const logEntry = JSON.parse(msg.trim());
                        const level = logEntry.level;
                        const message = logEntry.msg || "";
                        const method = logEntry.reqId ? logEntry.method : undefined;
                        const url = logEntry.reqId ? logEntry.url : undefined;
                        const statusCode = logEntry.res ? logEntry.res.statusCode : undefined;

                        let logMessage = message;
                        if (method && url) {
                            logMessage = `${method} ${url}`;
                            if (statusCode) {
                                logMessage += ` - ${statusCode}`;
                            }
                        }

                        // Map Pino levels to our log levels
                        if (level <= 20) {
                            // trace/debug
                            verbose(`[Fastify] ${logMessage}`);
                        } else if (level <= 30) {
                            // info
                            debug(`[Fastify] ${logMessage}`);
                        } else if (level <= 40) {
                            // warn
                            log(`[Fastify] ${logMessage}`);
                        } else {
                            // error/fatal
                            error(`[Fastify] ${logMessage}`);
                        }
                    } catch {
                        // If JSON parsing fails, just log the raw message
                        debug(`[Fastify] ${msg.trim()}`);
                    }
                }
            }
        };

        this.#fastify = Fastify({ logger: customLogger });
        this.#wss = new WebSocket.WebSocketServer({ noServer: true });
        this.#serviceDiscovery = new ServiceDiscovery();

        this.#setupWebSocket();
        this.#setupServiceDiscovery();
        this.#setupUpgradeHandling();
    }

    get port(): number {
        if (this.#port === null) {
            throw new Error("Server port is not available");
        }
        return this.#port;
    }

    get config(): Config {
        if (!this.#config) {
            throw new Error("Config is not set");
        }
        return this.#config;
    }

    set config(config: Config) {
        this.#config = config;
    }

    /** Set layout manager (for server role) */
    set layoutManager(layoutManager: LayoutManager | null) {
        this.#layoutManager = layoutManager;
    }

    /** Get layout manager */
    get layoutManager(): LayoutManager | null {
        return this.#layoutManager;
    }

    /** Get console instance for log broadcasting */
    get console(): ServerConsole {
        if (!this.#console) {
            throw new Error("Console instance is not available");
        }
        return this.#console;
    }

    /** Get service discovery instance for external access */
    get serviceDiscovery(): ServiceDiscovery {
        return this.#serviceDiscovery;
    }

    /** Set message handler for input events and instance info */
    setMessageHandler(
        handler: (message: InputEventMessage | InstanceInfoMessage | ClientRegistrationMessage) => void
    ): void {
        this.#messageHandler = handler;
    }

    /** Set disconnect handler for when a client disconnects */
    setDisconnectHandler(handler: (instanceId: string) => void): void {
        this.#disconnectHandler = handler;
    }

    /** Broadcast message to all regular WebSocket connections (excludes console connections) */
    broadcastToClients(message: string): void {
        for (const client of this.#regularConnections) {
            if (client.readyState === WebSocket.OPEN) {
                try {
                    client.send(message);
                } catch (err) {
                    verbose("Failed to send message to client:", err);
                }
            }
        }
    }

    /** Start server */
    async start(): Promise<void> {
        // this.#startHeartbeat();

        // Register API routes if config is available
        if (this.#config) {
            registerApiRoutes(this.#fastify, {
                config: this.#config,
                layoutManager: this.#layoutManager,
                role: this.#role,
                instanceId: this.#instanceId,
                startTime: this.startTime
            });
        }

        // Register static file serving for UI
        const uiPath = join(__dirname, "..", "ui");
        if (existsSync(uiPath)) {
            await this.#fastify.register(fastifyStatic, {
                root: uiPath,
                prefix: "/ui/"
            });

            // Redirect root to UI
            this.#fastify.get("/", async (_: FastifyRequest, reply: FastifyReply) => {
                return reply.redirect("/ui/");
            });

            verbose(`Serving UI from ${uiPath}`);
        } else {
            verbose(`UI not found at ${uiPath} - run 'npm run build:ui' to build it`);
        }

        let port = this.#port || 3000;

        while (true) {
            const opts: FastifyListenOptions = { port, host: "0.0.0.0" };
            try {
                const addr = await this.#fastify.listen(opts);
                debug(`HTTP listening at ${addr}, WS at ws://localhost:${port}/ws`);

                // Start service discovery after server is running
                // Only servers advertise themselves; clients just discover
                if (this.#role === "server") {
                    this.#serviceDiscovery.advertise(port, this.#instanceName, this.#role, this.startTime);
                }
                this.#serviceDiscovery.startDiscovery();
                break;
            } catch (err: unknown) {
                if (this.#port === null && port < 65535 && isEADDRINUSE(err)) {
                    verbose(`Port ${port} in use, trying next port...`);
                    ++port;
                    continue;
                }

                throw err;
            }
        }
        if (this.#port === null) {
            this.#port = port;
        }

        this.#console = new ServerConsole(
            port,
            this.#quitHandler,
            this.config,
            this.#instanceId,
            this.#instanceName,
            this.#version,
            this.#capabilities
        );

        process.on("SIGINT", (): void => {
            this.stop();
        });
        process.on("SIGTERM", (): void => {
            this.stop();
        });
    }

    /** Stop server gracefully */
    async stop(): Promise<void> {
        if (this.#heartbeatInterval !== undefined) {
            clearInterval(this.#heartbeatInterval);
            this.#heartbeatInterval = undefined;
        }

        // Stop service discovery
        await this.#serviceDiscovery.stop();

        this.#wss.clients.forEach((c: WebSocket.WebSocket): void => {
            c.terminate();
        });
        this.#wss.close();

        await this.#fastify.close();
        log("Server shut down");
        process.exit(0);
    }

    // /** Register HTTP routes */
    // #registerRoutes(): void {
    //     this.#fastify.get("/", async () => {
    //         return { message: "Hello from Fastify + ws + TypeScript + Class!" };
    //     });

    //     this.#fastify.post<{ Body: unknown }>("/api/echo", async (request: string) => {
    //         return { echoed: request.body };
    //     });
    // }

    /** Setup service discovery event handlers */
    #setupServiceDiscovery(): void {
        this.#serviceDiscovery.on("serviceUp", (service: DiscoveredService) => {
            log(`New Konflikt instance discovered: ${service.name} at ${service.host}:${service.port}`);
            // You can add logic here to establish connections with discovered services
        });

        this.#serviceDiscovery.on("serviceDown", (service: DiscoveredService) => {
            log(`Konflikt instance went offline: ${service.name}`);
            // You can add cleanup logic here
        });
    }

    /** Setup WebSocket server events */
    #setupWebSocket(): void {
        this.#wss.on("connection", (socket: WebSocket, req: IncomingMessage) => {
            debug(`WebSocket connection opened from ${req.socket.remoteAddress} to ${req.url}`);

            // Check if this is a console connection
            const isConsoleConnection = req.url?.startsWith("/console");

            if (isConsoleConnection) {
                this.console.handleConsoleConnection(socket, req, this.#wss);
            } else {
                this.#handleRegularConnection(socket, req);
            }
        });
    }

    #handleRegularConnection(socket: WebSocket, req: IncomingMessage): void {
        verbose(`Handling regular WS connection from ${req.socket.remoteAddress}`);

        // Add to regular connections set
        this.#regularConnections.add(socket);

        socket.on("message", (data: WebSocket.RawData) => {
            const text = textFromWebSocketMessage(data);

            try {
                const parsed = JSON.parse(text);

                // Handle handshake request
                if (parsed.type === "handshake_request") {
                    const request = parsed as HandshakeRequest;
                    const serverCommit = getGitCommit();
                    log(`Received handshake request from ${request.instanceName} (${request.instanceId}), client commit: ${request.gitCommit || "unknown"}, server commit: ${serverCommit || "unknown"}`);

                    const response: HandshakeResponse = {
                        ...createBaseMessage(this.#instanceId),
                        type: "handshake_response",
                        accepted: true,
                        instanceId: this.#instanceId,
                        instanceName: this.#instanceName,
                        version: this.#version,
                        capabilities: this.#capabilities,
                        gitCommit: serverCommit
                    };

                    socket.send(JSON.stringify(response));
                    log(`Sent handshake response to ${request.instanceName}`);

                    // Track this socket's instance ID for disconnect handling
                    this.#socketToInstanceId.set(socket, request.instanceId);

                    // Check if client needs to update
                    if (serverCommit && request.gitCommit && request.gitCommit !== serverCommit) {
                        log(`Client ${request.instanceName} has different commit (${request.gitCommit}), sending update_required`);
                        const updateMessage = {
                            type: "update_required",
                            serverCommit,
                            clientCommit: request.gitCommit,
                            timestamp: Date.now()
                        };
                        socket.send(JSON.stringify(updateMessage));
                    }
                    return;
                }

                // Handle restart request (client is asking server to restart for update)
                if (typeof parsed === "object" && parsed !== null && "type" in parsed && parsed.type === "restart_request") {
                    log(`Received restart request from client - exiting with code 43 for update`);
                    process.exit(43);
                }

                // Handle input event, client registration, and deactivation messages
                if (
                    isInputEventMessage(parsed) ||
                    isInstanceInfoMessage(parsed) ||
                    isClientRegistrationMessage(parsed) ||
                    (typeof parsed === "object" && parsed !== null && "type" in parsed && parsed.type === "deactivation_request")
                ) {
                    if (this.#messageHandler) {
                        this.#messageHandler(parsed);
                    } else {
                        verbose("Received message but no message handler set:", parsed.type);
                    }
                } else {
                    verbose("Received unknown message type:", parsed);
                }
            } catch (err) {
                verbose("Failed to parse WebSocket message:", err);
            }
        });

        socket.on("close", () => {
            verbose(`Regular WS connection closed from ${req.socket.remoteAddress}`);
            this.#regularConnections.delete(socket);

            // Notify about disconnect if we know the instance ID
            const instanceId = this.#socketToInstanceId.get(socket);
            if (instanceId) {
                log(`Client ${instanceId} disconnected`);
                this.#socketToInstanceId.delete(socket);
                if (this.#disconnectHandler) {
                    this.#disconnectHandler(instanceId);
                }
            }
        });

        socket.on("error", (err: Error) => {
            verbose(`Regular WS connection error from ${req.socket.remoteAddress}:`, err);
            this.#regularConnections.delete(socket);

            // Notify about disconnect if we know the instance ID
            const instanceId = this.#socketToInstanceId.get(socket);
            if (instanceId) {
                log(`Client ${instanceId} disconnected due to error`);
                this.#socketToInstanceId.delete(socket);
                if (this.#disconnectHandler) {
                    this.#disconnectHandler(instanceId);
                }
            }
        });
    }

    #setupUpgradeHandling(): void {
        this.#fastify.server.on("upgrade", (request: IncomingMessage, socket: Duplex, head: Buffer): void => {
            const url = request.url || "";
            if (url.startsWith("/ws") || url.startsWith("/console")) {
                this.#wss.handleUpgrade(request, socket, head, (ws: WebSocket): void => {
                    this.#wss.emit("connection", ws, request);
                });
            } else {
                socket.write("HTTP/1.1 400 Bad Request\\r\\n\\r\\n");
                socket.destroy();
            }
        });
    }

    // #startHeartbeat(): void {
    //     const intervalMs = 30_000;
    //     this.heartbeatInterval = setInterval(() => {
    //         this.wss.clients.forEach((client: WebSocket.WebSocket): void => {
    //             const c = client as HeartbeatWebSocket;
    //             if (c.isAlive === false) {
    //                 c.terminate();
    //                 return;
    //             }
    //             c.isAlive = false;
    //             c.ping();
    //         });
    //     }, intervalMs);
    // }
}
