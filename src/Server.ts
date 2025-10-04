import { ServiceDiscovery } from "./ServiceDiscovery";
import { debug, error, log, verbose } from "./Log";
import Fastify from "fastify";
import WebSocket from "ws";
import type { DiscoveredService } from "./DiscoveredService";
import type { FastifyInstance, FastifyListenOptions } from "fastify";
import type { IncomingMessage } from "http";

export class Server {
    #fastify: FastifyInstance;
    #wss: WebSocket.WebSocketServer;
    #heartbeatInterval: ReturnType<typeof setInterval> | undefined = undefined;
    #serviceDiscovery: ServiceDiscovery;

    constructor(readonly port: number = 3000) {
        // Create a custom logger that integrates with our logging system
        const customLogger = {
            level: 'debug',
            stream: {
                write: (msg: string): void => {
                    // Fastify logs are JSON, try to parse and format them nicely
                    try {
                        const logEntry = JSON.parse(msg.trim());
                        const level = logEntry.level;
                        const message = logEntry.msg || '';
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
                        if (level <= 20) { // trace/debug
                            verbose(`[Fastify] ${logMessage}`);
                        } else if (level <= 30) { // info
                            debug(`[Fastify] ${logMessage}`);
                        } else if (level <= 40) { // warn
                            log(`[Fastify] ${logMessage}`);
                        } else { // error/fatal
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

        // this.#registerRoutes();
        this.#setupWebSocket();
        this.#setupServiceDiscovery();
        // this.#setupUpgradeHandling();
    }

    /** Get service discovery instance for external access */
    get serviceDiscovery(): ServiceDiscovery {
        return this.#serviceDiscovery;
    }

    /** Start server */
    async start(): Promise<void> {
        // this.#startHeartbeat();

        const opts: FastifyListenOptions = { port: this.port, host: "0.0.0.0" };
        try {
            const addr = await this.#fastify.listen(opts);
            log(`HTTP listening at ${addr}, WS at ws://localhost:${this.port}/ws`);

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

    // /** Setup WebSocket server events */
    #setupWebSocket(): void {
        this.#wss.on("connection", (socket: WebSocket, req: IncomingMessage) => {
            debug(`WebSocket connection opened from ${req.socket.remoteAddress} to ${req.url}`);

            socket.on("message", (_raw: WebSocket.RawData) => {
                // let text: string;
                // if (raw instanceof Buffer) {
                //     text = raw.toString("utf-8");
                // } else if (raw instanceof ArrayBuffer) {
                //     text = Buffer.from(raw).toString("utf-8");
                // } else {
                //     assert(Array.isArray(raw));
                //     text = Buffer.concat(...raw).toString("utf-8");
                // }
                // this.#fastify.log.info({ msg: text }, "Received WS message");
                // // Try JSON echo
                // try {
                //     const parsed: unknown = JSON.parse(text);
                //     if (typeof parsed === "object" && parsed && "type" in parsed && parsed.type === "echo") {
                //         socket.send(JSON.stringify({ type: "echo", data: ("data" in parsed ? parsed.data : undefined }));
                //         return;
                //     }
                // } catch {
                //     // ignore parse error, just broadcast text
                // }
                // // Broadcast to all clients
                // this.wss.clients.forEach((client) => {
                //     const c = client as HeartbeatWebSocket;
                //     if (c.readyState === WebSocket.OPEN) {
                //         c.send(
                //             JSON.stringify({
                //                 from: req.socket.remoteAddress,
                //                 message: text
                //             })
                //         );
                //     }
                // });
            });

            socket.on("close", () => {
                // this.#fastify.log.info("WebSocket connection closed");
            });

            socket.on("error", (_err: Error) => {
                // this.#fastify.log.error(err, "WebSocket error");
            });

            // Welcome message
            // socket.send(JSON.stringify({ type: "welcome", message: "connected to /ws" }));
        });
    }

    // #setupUpgradeHandling(): void {
    //     this.#fastify.server.on("upgrade", (request, socket, head) => {
    //         if (request.url?.startsWith("/ws")) {
    //             this.wss.handleUpgrade(request, socket, head, (ws) => {
    //                 this.wss.emit("connection", ws, request);
    //             });
    //         } else {
    //             socket.write("HTTP/1.1 400 Bad Request\r\n\r\n");
    //             socket.destroy();
    //         }
    //     });
    // }

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
