import { ServerConsole } from "./ServerConsole";
import { ServiceDiscovery } from "./ServiceDiscovery";
import { debug, error, log, verbose } from "./Log";
import Fastify from "fastify";
import WebSocket from "ws";
import type { Config } from "./Config";
import type { DiscoveredService } from "./DiscoveredService";
import type { Duplex } from "stream";
import type { FastifyInstance, FastifyListenOptions } from "fastify";
import type { IncomingMessage } from "http";

export class Server {
    #fastify: FastifyInstance;
    #wss: WebSocket.WebSocketServer;
    #heartbeatInterval: ReturnType<typeof setInterval> | undefined = undefined;
    #serviceDiscovery: ServiceDiscovery;
    #instanceId: string;
    #instanceName: string;
    #version: string;
    #capabilities: string[];
    #console: ServerConsole;

    constructor(
        readonly port: number = 3000,
        instanceId?: string,
        instanceName?: string,
        version: string = "1.0.0",
        capabilities: string[] = ["input_events", "state_sync"]
    ) {
        this.#instanceId = instanceId || crypto.randomUUID();
        this.#instanceName = instanceName || `konflikt-${process.pid}`;
        this.#version = version;
        this.#capabilities = capabilities;
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
        this.#console = new ServerConsole(
            this.port,
            this.#instanceId,
            this.#instanceName,
            this.#version,
            this.#capabilities
        );

        this.#setupWebSocket();
        this.#setupServiceDiscovery();
        this.#setupUpgradeHandling();
    }

    /** Get service discovery instance for external access */
    get serviceDiscovery(): ServiceDiscovery {
        return this.#serviceDiscovery;
    }

    /** Set config for console commands */
    setConfig(config: Config): void {
        this.#console.setConfig(config);
    }

    /** Start server */
    async start(): Promise<void> {
        // this.#startHeartbeat();

        const opts: FastifyListenOptions = { port: this.port, host: "0.0.0.0" };
        try {
            const addr = await this.#fastify.listen(opts);
            debug(`HTTP listening at ${addr}, WS at ws://localhost:${this.port}/ws`);

            // Start service discovery after server is running
            this.#serviceDiscovery.advertise(this.port);
            this.#serviceDiscovery.startDiscovery();
        } catch (err: unknown) {
            error("Failed to start server:", err);
            process.exit(1);
        }

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
                this.#console.handleConsoleConnection(socket, req, this.#wss);
            } else {
                this.#handleRegularConnection(socket, req);
            }
        });
    }

    #handleRegularConnection(socket: WebSocket, req: IncomingMessage): void {
        verbose(`Handling regular WS connection from ${req.socket.remoteAddress}`, typeof this);
        socket.on("message", (_raw: WebSocket.RawData) => {
            // TODO: Add message handling logic when needed
        });

        socket.on("close", () => {
            // TODO: Handle connection close
        });

        socket.on("error", (_err: Error) => {
            // TODO: Handle connection error
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
