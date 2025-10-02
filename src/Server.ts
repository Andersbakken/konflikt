import Fastify, { FastifyInstance, FastifyListenOptions } from "fastify";
import WebSocket, { WebSocketServer } from "ws";
import { IncomingMessage } from "http";

// Extend WebSocket type with heartbeat tracking
interface HeartbeatWebSocket extends WebSocket {
    isAlive?: boolean;
}

export class Server {
    private fastify: FastifyInstance;
    private wss: WebSocketServer;
    private heartbeatInterval: NodeJS.Timer | null = null;
    private readonly port: number;

    constructor(port: number = 3000) {
        this.port = port;
        this.fastify = Fastify({ logger: true });
        this.wss = new WebSocketServer({ noServer: true });

        this.registerRoutes();
        this.setupWebSocket();
        this.setupUpgradeHandling();
    }

    /** Register HTTP routes */
    private registerRoutes(): void {
        this.fastify.get("/", async () => {
            return { message: "Hello from Fastify + ws + TypeScript + Class!" };
        });

        this.fastify.post<{ Body: unknown }>("/api/echo", async (request) => {
            return { echoed: request.body };
        });
    }

    /** Setup WebSocket server events */
    private setupWebSocket(): void {
        this.wss.on("connection", (socket: HeartbeatWebSocket, req: IncomingMessage) => {
            socket.isAlive = true;
            socket.on("pong", () => (socket.isAlive = true));

            this.fastify.log.info({ url: req.url }, "WebSocket connection opened");

            socket.on("message", (raw: WebSocket.RawData) => {
                const text = raw.toString();
                this.fastify.log.info({ msg: text }, "Received WS message");

                // Try JSON echo
                try {
                    const parsed: any = JSON.parse(text);
                    if (parsed?.type === "echo") {
                        socket.send(JSON.stringify({ type: "echo", data: parsed.data }));
                        return;
                    }
                } catch {
                    // ignore parse error, just broadcast text
                }

                // Broadcast to all clients
                this.wss.clients.forEach((client) => {
                    const c = client as HeartbeatWebSocket;
                    if (c.readyState === WebSocket.OPEN) {
                        c.send(
                            JSON.stringify({
                                from: req.socket.remoteAddress,
                                message: text
                            })
                        );
                    }
                });
            });

            socket.on("close", () => {
                this.fastify.log.info("WebSocket connection closed");
            });

            socket.on("error", (err: Error) => {
                this.fastify.log.error(err, "WebSocket error");
            });

            // Welcome message
            socket.send(JSON.stringify({ type: "welcome", message: "connected to /ws" }));
        });
    }

    /** Handle HTTP upgrade for WebSocket connections */
    private setupUpgradeHandling(): void {
        this.fastify.server.on("upgrade", (request, socket, head) => {
            if (request.url?.startsWith("/ws")) {
                this.wss.handleUpgrade(request, socket, head, (ws) => {
                    this.wss.emit("connection", ws, request);
                });
            } else {
                socket.write("HTTP/1.1 400 Bad Request\r\n\r\n");
                socket.destroy();
            }
        });
    }

    /** Start heartbeat to terminate dead sockets */
    private startHeartbeat(): void {
        const intervalMs = 30_000;
        this.heartbeatInterval = setInterval(() => {
            this.wss.clients.forEach((client) => {
                const c = client as HeartbeatWebSocket;
                if (c.isAlive === false) {
                    c.terminate();
                    return;
                }
                c.isAlive = false;
                c.ping();
            });
        }, intervalMs);
    }

    /** Start server */
    public async start(): Promise<void> {
        this.startHeartbeat();

        const opts: FastifyListenOptions = { port: this.port, host: "0.0.0.0" };
        try {
            const addr = await this.fastify.listen(opts);
            this.fastify.log.info(`HTTP listening at ${addr}, WS at ws://localhost:${this.port}/ws`);
        } catch (err) {
            this.fastify.log.error(err);
            process.exit(1);
        }

        process.on("SIGINT", () => this.stop());
        process.on("SIGTERM", () => this.stop());
    }

    /** Stop server gracefully */
    public async stop(): Promise<void> {
        if (this.heartbeatInterval) {
            clearInterval(this.heartbeatInterval);
            this.heartbeatInterval = null;
        }

        this.wss.clients.forEach((c) => c.terminate());
        this.wss.close();

        await this.fastify.close();
        this.fastify.log.info("Server shut down");
        process.exit(0);
    }
}
