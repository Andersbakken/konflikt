import { z } from "zod";

export const ConsoleLogMessageSchema = z.object({
    type: z.literal("console_log"),
    level: z.enum(["verbose", "debug", "log", "error"]),
    message: z.string(),
    timestamp: z.number().optional()
});