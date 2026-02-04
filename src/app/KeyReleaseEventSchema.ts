import { BaseMessageSchema } from "./BaseMessageSchema";
import { StateSchema } from "./StateSchema";
import { z } from "zod";

export const KeyReleaseEventSchema = BaseMessageSchema.extend({
    type: z.literal("key_release"),
    keycode: z.number(),
    text: z.string().optional(),
    state: StateSchema
});
