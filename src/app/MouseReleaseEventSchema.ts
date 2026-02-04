import { BaseMessageSchema } from "./BaseMessageSchema";
import { MouseButtonSchema } from "./MouseButtonSchema";
import { StateSchema } from "./StateSchema";
import { z } from "zod";

export const MouseReleaseEventSchema = BaseMessageSchema.extend({
    type: z.literal("mouse_release"),
    button: MouseButtonSchema,
    state: StateSchema
});
