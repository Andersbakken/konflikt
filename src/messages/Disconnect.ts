import { BaseMessageSchema } from "./BaseMessage";
import { z } from "zod";

export const DisconnectSchema = BaseMessageSchema.extend({
    type: z.literal("disconnect"),
    reason: z.string().optional(),
});
export type Disconnect = z.infer<typeof DisconnectSchema>;