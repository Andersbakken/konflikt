import { z } from "zod";

export const KeyboardModifierSchema = z.object({
    leftShift: z.boolean().default(false),
    rightShift: z.boolean().default(false),
    leftAlt: z.boolean().default(false),
    rightAlt: z.boolean().default(false),
    leftControl: z.boolean().default(false),
    rightControl: z.boolean().default(false),
    leftSuper: z.boolean().default(false),
    rightSuper: z.boolean().default(false),
    capsLock: z.boolean().default(false),
    numLock: z.boolean().default(false),
    scrollLock: z.boolean().default(false)
});
export type KeyboardModifier = z.infer<typeof KeyboardModifierSchema>;
