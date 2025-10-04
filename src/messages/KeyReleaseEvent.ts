import { BaseMessageSchema } from "./BaseMessage";
import { StateSchema } from "./State";
import { z } from "zod";

export const KeyReleaseEventSchema = BaseMessageSchema.extend({
    type: z.literal("key_release"),
    keycode: z.number(),
    text: z.string().optional(),
    state: StateSchema,
});
export type KeyReleaseEvent = z.infer<typeof KeyReleaseEventSchema>;