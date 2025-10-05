import { BaseMessageSchema } from "./BaseMessageSchema";
import { StateSchema } from "./StateSchema";
import { z } from "zod";

export const KeyPressEventSchema = BaseMessageSchema.extend({
    type: z.literal("key_press"),
    keycode: z.number(),
    text: z.string().optional(),
    state: StateSchema
});
