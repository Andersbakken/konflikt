import type { Config } from "./Config";
import type { FastifyInstance, FastifyReply, FastifyRequest } from "fastify";
import type { LayoutManager } from "./LayoutManager";
import type { ScreenEntry } from "./ScreenEntry";

interface ApiDependencies {
    config: Config;
    layoutManager: LayoutManager | null;
    role: string;
    instanceId: string;
    startTime: number;
}

interface LayoutUpdateBody {
    screens: Array<{ instanceId: string; x: number; y: number }>;
}

interface PositionUpdateBody {
    x: number;
    y: number;
}

interface ConfigUpdateBody {
    displayName?: string;
    // Add more config options as needed
}

export function registerApiRoutes(fastify: FastifyInstance, deps: ApiDependencies): void {
    const { config, layoutManager, role, instanceId, startTime } = deps;

    // Status endpoint - available on both server and client
    fastify.get("/api/status", async () => {
        return {
            role,
            instanceId,
            instanceName: config.instanceName,
            startTime,
            uptime: Date.now() - startTime,
            port: config.port,
            version: "1.0.0"
        };
    });

    // Config endpoints - available on both server and client
    fastify.get("/api/config", async () => {
        return {
            displayName: config.instanceName,
            role,
            logLevel: config.logLevel,
            discoveryEnabled: config.discoveryEnabled
        };
    });

    fastify.put<{ Body: ConfigUpdateBody }>(
        "/api/config",
        async (
            request: FastifyRequest<{ Body: ConfigUpdateBody }>,
            reply: FastifyReply
        ) => {
            // Config updates would require modifying the config file
            // For now, just acknowledge the request
            const { displayName } = request.body;
            if (displayName) {
                // In a full implementation, we'd update the config file
                return { success: true, message: "Config update not yet implemented" };
            }
            return reply.code(400).send({ error: "No valid config options provided" });
        }
    );

    // Layout endpoints - only available on server
    if (role === "server" && layoutManager) {
        fastify.get("/api/layout", async () => {
            return {
                screens: layoutManager.getLayout()
            };
        });

        fastify.put<{ Body: LayoutUpdateBody }>(
            "/api/layout",
            async (
                request: FastifyRequest<{ Body: LayoutUpdateBody }>,
                _reply: FastifyReply
            ) => {
                const { screens } = request.body;
                layoutManager.updateLayout(screens);
                return {
                    success: true,
                    screens: layoutManager.getLayout()
                };
            }
        );

        fastify.patch<{ Params: { id: string }; Body: PositionUpdateBody }>(
            "/api/layout/:id",
            async (
                request: FastifyRequest<{ Params: { id: string }; Body: PositionUpdateBody }>,
                reply: FastifyReply
            ) => {
                const { id } = request.params;
                const { x, y } = request.body;

                const screen = layoutManager.getScreen(id);
                if (!screen) {
                    return reply.code(404).send({ error: "Screen not found" });
                }

                if (typeof x !== "number" || typeof y !== "number") {
                    return reply.code(400).send({ error: "Invalid position data" });
                }

                layoutManager.updatePosition(id, x, y);
                return {
                    success: true,
                    screen: layoutManager.getScreen(id)
                };
            }
        );

        // Remove offline client from layout
        fastify.delete<{ Params: { id: string } }>(
            "/api/layout/:id",
            async (
                request: FastifyRequest<{ Params: { id: string } }>,
                reply: FastifyReply
            ) => {
                const { id } = request.params;
                const screen = layoutManager.getScreen(id);

                if (!screen) {
                    return reply.code(404).send({ error: "Screen not found" });
                }

                if (screen.isServer) {
                    return reply.code(400).send({ error: "Cannot remove server screen" });
                }

                if (screen.online) {
                    return reply.code(400).send({ error: "Cannot remove online client" });
                }

                layoutManager.removeClient(id);
                return { success: true };
            }
        );
    } else if (role === "client") {
        // Client-side layout view (read-only)
        fastify.get("/api/layout", async () => {
            // Client would need to store the layout received from server
            // For now, return empty or a placeholder
            return {
                screens: [] as ScreenEntry[],
                message: "Layout is managed by server"
            };
        });
    }
}
