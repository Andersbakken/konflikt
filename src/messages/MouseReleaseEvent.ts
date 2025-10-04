import { BaseMessageSchema } from "./BaseMessage";
import { MouseButtonSchema } from "./MouseButton";
import { StateSchema } from "./State";
import { z } from "zod";

export const MouseReleaseEventSchema = BaseMessageSchema.extend({
    type: z.literal("mouse_release"),
    button: MouseButtonSchema,
    state: StateSchema,
});
export type MouseReleaseEvent = z.infer<typeof MouseReleaseEventSchema>;