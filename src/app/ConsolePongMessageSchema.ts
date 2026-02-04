import { z } from "zod";

export const ConsolePongMessageSchema = z.object({
    type: z.literal("pong"),
    timestamp: z.number().optional()
});