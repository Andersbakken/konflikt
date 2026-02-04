import { z } from "zod";

export const InputEventMessageSchema = z.object({
    type: z.literal("input_event"),
    sourceInstanceId: z.string(),
    sourceDisplayId: z.string(),
    sourceMachineId: z.string(),
    eventType: z.enum(["keyPress", "keyRelease", "mousePress", "mouseRelease", "mouseMove"]),
    eventData: z.object({
        x: z.number(),
        y: z.number(),
        timestamp: z.number(),
        keyboardModifiers: z.number(),
        mouseButtons: z.number(),
        keycode: z.number().optional(),
        text: z.string().optional(),
        button: z.number().optional(),
        dx: z.number().optional(),
        dy: z.number().optional()
    })
});
