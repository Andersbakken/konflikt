import { WebSocketServer } from "ws";
import Fastify from "fastify";
import type { FastifyInstance, FastifyListenOptions } from "fastify";
// import type { IncomingMessage } from "http";
import type WebSocket from "ws";

// Extend WebSocket type with heartbeat tracking
// interface HeartbeatWebSocket extends WebSocket {
//     isAlive?: boolean;
// }

export class Server {
    #fastify: FastifyInstance;
    #wss: WebSocketServer;
    #heartbeatInterval: ReturnType<typeof setInterval> | undefined = undefined;

    constructor(readonly port: number = 3000) {
        this.#fastify = Fastify({ logger: true });
        this.#wss = new WebSocketServer({ noServer: true });

        // this.#registerRoutes();
        // this.#setupWebSocket();
        // this.#setupUpgradeHandling();
    }

    /** Start server */
    async start(): Promise<void> {
        // this.#startHeartbeat();

        const opts: FastifyListenOptions = { port: this.port, host: "0.0.0.0" };
        try {
            const addr = await this.#fastify.listen(opts);
            this.#fastify.log.info(`HTTP listening at ${addr}, WS at ws://localhost:${this.port}/ws`);
        } catch (err) {
            this.#fastify.log.error(err);
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

        this.#wss.clients.forEach((c: WebSocket.WebSocket): void => {
            c.terminate();
        });
        this.#wss.close();

        await this.#fastify.close();
        this.#fastify.log.info("Server shut down");
        process.exit(0);
    }

    // /** Register HTTP routes */
    // #registerRoutes(): void {
    //     this.fastify.get("/", async () => {
    //         return { message: "Hello from Fastify + ws + TypeScript + Class!" };
    //     });

    //     this.fastify.post<{ Body: unknown }>("/api/echo", async (request: string) => {
    //         return { echoed: request.body };
    //     });
    // }

    // /** Setup WebSocket server events */
    // #setupWebSocket(): void {
    //     this.wss.on("connection", (socket: HeartbeatWebSocket, req: IncomingMessage) => {
    //         socket.isAlive = true;
    //         socket.on("pong", () => (socket.isAlive = true));

    //         this.fastify.log.info({ url: req.url }, "WebSocket connection opened");

    //         socket.on("message", (raw: WebSocket.RawData) => {
    //             let text: string;
    //             if (raw instanceof Buffer) {
    //                 text = raw.toString("utf-8");
    //             } else if (raw instanceof ArrayBuffer) {
    //                 text = Buffer.from(raw).toString("utf-8");
    //             } else {
    //                 assert(Array.isArray(raw));
    //                 text = Buffer.concat(...raw).toString("utf-8");
    //             }
    //             this.fastify.log.info({ msg: text }, "Received WS message");

    //             // Try JSON echo
    //             try {
    //                 const parsed: unknown = JSON.parse(text);
    //                 if (typeof parsed === "object" && parsed && "type" in parsed && parsed.type === "echo") {
    //                     socket.send(JSON.stringify({ type: "echo", data: ("data" in parsed ? parsed.data : undefined  }));
    //                     return;
    //                 }
    //             } catch {
    //                 // ignore parse error, just broadcast text
    //             }

    //             // Broadcast to all clients
    //             this.wss.clients.forEach((client) => {
    //                 const c = client as HeartbeatWebSocket;
    //                 if (c.readyState === WebSocket.OPEN) {
    //                     c.send(
    //                         JSON.stringify({
    //                             from: req.socket.remoteAddress,
    //                             message: text
    //                         })
    //                     );
    //                 }
    //             });
    //         });

    //         socket.on("close", () => {
    //             this.fastify.log.info("WebSocket connection closed");
    //         });

    //         socket.on("error", (err: Error) => {
    //             this.fastify.log.error(err, "WebSocket error");
    //         });

    //         // Welcome message
    //         socket.send(JSON.stringify({ type: "welcome", message: "connected to /ws" }));
    //     });
    // }

    // #setupUpgradeHandling(): void {
    //     this.fastify.server.on("upgrade", (request, socket, head) => {
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
