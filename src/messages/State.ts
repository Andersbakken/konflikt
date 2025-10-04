import { KeyboardModifierSchema } from "./KeyboardModifier";
import { z } from "zod";

export const StateSchema = z.object({
    x: z.number(),
    y: z.number(),
    keyboardModifiers: KeyboardModifierSchema,
    mouseButtons: z.object({
        left: z.boolean().default(false),
        right: z.boolean().default(false),
        middle: z.boolean().default(false),
    }),
});
export type State = z.infer<typeof StateSchema>;