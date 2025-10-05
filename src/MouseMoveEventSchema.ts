import { BaseMessageSchema } from "./BaseMessageSchema";
import { StateSchema } from "./StateSchema";
import { z } from "zod";

export const MouseMoveEventSchema = BaseMessageSchema.extend({
    type: z.literal("mouse_move"),
    state: StateSchema
});
