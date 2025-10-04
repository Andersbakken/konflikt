import { BaseMessageSchema } from "./BaseMessage";
import { z } from "zod";

export const HeartbeatSchema = BaseMessageSchema.extend({
    type: z.literal("heartbeat")
});
export type Heartbeat = z.infer<typeof HeartbeatSchema>;
