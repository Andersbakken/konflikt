import { z } from "zod";

export const ConsoleCommandMessageSchema = z.object({
    type: z.literal("console_command"),
    command: z.string(),
    args: z.array(z.string()),
    timestamp: z.number().optional()
});