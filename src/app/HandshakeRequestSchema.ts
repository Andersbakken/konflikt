import { BaseMessageSchema } from "./BaseMessageSchema";
import { z } from "zod";

const ScreenGeometrySchema = z.object({
    x: z.number(),
    y: z.number(),
    width: z.number(),
    height: z.number()
});

export const HandshakeRequestSchema = BaseMessageSchema.extend({
    type: z.literal("handshake_request"),
    instanceId: z.string(),
    instanceName: z.string(),
    version: z.string(),
    capabilities: z.array(z.string()),
    screenGeometry: ScreenGeometrySchema.optional(),
    gitCommit: z.string().optional()
});
