import { BaseMessageSchema } from "./BaseMessageSchema";
import { z } from "zod";

export const HandshakeResponseSchema = BaseMessageSchema.extend({
    type: z.literal("handshake_response"),
    accepted: z.boolean(),
    instanceId: z.string(),
    instanceName: z.string(),
    version: z.string(),
    capabilities: z.array(z.string()),
    reason: z.string().optional(), // If rejected
    gitCommit: z.string().optional()
});
