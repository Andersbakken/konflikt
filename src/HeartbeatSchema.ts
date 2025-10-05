import { BaseMessageSchema } from "./BaseMessageSchema";
import { z } from "zod";

export const HeartbeatSchema = BaseMessageSchema.extend({
    type: z.literal("heartbeat")
});
