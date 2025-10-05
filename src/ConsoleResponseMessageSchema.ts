import { z } from "zod";

export const ConsoleResponseMessageSchema = z.object({
    type: z.literal("console_response"),
    output: z.string()
});