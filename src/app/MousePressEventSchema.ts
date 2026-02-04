import { BaseMessageSchema } from "./BaseMessageSchema";
import { MouseButtonSchema } from "./MouseButtonSchema";
import { StateSchema } from "./StateSchema";
import { z } from "zod";

export const MousePressEventSchema = BaseMessageSchema.extend({
    type: z.literal("mouse_press"),
    button: MouseButtonSchema,
    state: StateSchema
});
