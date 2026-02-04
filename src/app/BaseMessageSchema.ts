import { z } from "zod";

export const BaseMessageSchema = z.object({
    id: z.string().uuid(),
    timestamp: z.number(),
    source: z.string() // Instance ID that sent the message
});
