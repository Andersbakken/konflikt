import { ServerConsole } from "./ServerConsole";
import { ServiceDiscovery } from "./ServiceDiscovery";
import { debug } from "./debug";
import { error } from "./error";
import { isEADDRINUSE } from "./isEADDRINUSE";
import { isInputEventMessage } from "./isInputEventMessage";
import { isInstanceInfoMessage } from "./isInstanceInfoMessage";
import { log } from "./log";
import { textFromWebSocketMessage } from "./textFromWebSocketMessage";
import { verbose } from "./verbose";
import Fastify from "fastify";
import WebSocket from "ws";
import type { Config } from "./Config";
import type { DiscoveredService } from "./DiscoveredService";
import type { Duplex } from "stream";
import type { FastifyInstance, FastifyListenOptions } from "fastify";
import type { IncomingMessage } from "http";
import type { InputEventMessage } from "./InputEventMessage";
import type { InstanceInfoMessage } from "./InstanceInfoMessage";

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

    // Message handler callback for input events
    #messageHandler?: (message: InputEventMessage | InstanceInfoMessage) => void;

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
    setMessageHandler(handler: (message: InputEventMessage | InstanceInfoMessage) => void): void {
        this.#messageHandler = handler;
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

        let port = this.#port || 3000;

        while (true) {
            const opts: FastifyListenOptions = { port, host: "0.0.0.0" };
            try {
                const addr = await this.#fastify.listen(opts);
                debug(`HTTP listening at ${addr}, WS at ws://localhost:${port}/ws`);

                // Start service discovery after server is running
                this.#serviceDiscovery.advertise(port, this.#instanceName, this.#role, this.startTime);
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

                // Handle input event messages
                if (isInputEventMessage(parsed) || isInstanceInfoMessage(parsed)) {
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
        });

        socket.on("error", (err: Error) => {
            verbose(`Regular WS connection error from ${req.socket.remoteAddress}:`, err);
            this.#regularConnections.delete(socket);
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
