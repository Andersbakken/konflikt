import { z } from "zod";

export const ActivateClientMessageSchema = z.object({
    type: z.literal("activate_client"),
    targetInstanceId: z.string(),
    cursorX: z.number(),
    cursorY: z.number(),
    timestamp: z.number()
});
