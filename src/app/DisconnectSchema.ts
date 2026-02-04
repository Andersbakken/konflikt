import { BaseMessageSchema } from "./BaseMessageSchema";
import { z } from "zod";

export const DisconnectSchema = BaseMessageSchema.extend({
    type: z.literal("disconnect"),
    reason: z.string().optional()
});
