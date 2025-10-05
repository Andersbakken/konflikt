import { BaseMessageSchema } from "./BaseMessage";
import { StateSchema } from "./State";
import { z } from "zod";

export const KeyPressEventSchema = BaseMessageSchema.extend({
    type: z.literal("key_press"),
    keycode: z.number(),
    text: z.string().optional(),
    state: StateSchema
});
export type KeyPressEvent = z.infer<typeof KeyPressEventSchema>;
