import { z } from "zod";

export const ConsoleErrorMessageSchema = z.object({
    type: z.literal("console_error"),
    error: z.string()
});