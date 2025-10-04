import { BaseMessageSchema } from "./BaseMessage";
import { z } from "zod";

export const HandshakeRequestSchema = BaseMessageSchema.extend({
    type: z.literal("handshake_request"),
    instanceId: z.string(),
    instanceName: z.string(),
    version: z.string(),
    capabilities: z.array(z.string())
});
export type HandshakeRequest = z.infer<typeof HandshakeRequestSchema>;
