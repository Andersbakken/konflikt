import { BaseMessageSchema } from "./BaseMessage";
import { MouseButtonSchema } from "./MouseButton";
import { StateSchema } from "./State";
import { z } from "zod";

export const MousePressEventSchema = BaseMessageSchema.extend({
    type: z.literal("mouse_press"),
    button: MouseButtonSchema,
    state: StateSchema
});
export type MousePressEvent = z.infer<typeof MousePressEventSchema>;
