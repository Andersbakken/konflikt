import { BaseMessageSchema } from "./BaseMessage";
import { StateSchema } from "./State";
import { z } from "zod";

export const MouseMoveEventSchema = BaseMessageSchema.extend({
    type: z.literal("mouse_move"),
    state: StateSchema,
});
export type MouseMoveEvent = z.infer<typeof MouseMoveEventSchema>;