import { BaseMessageSchema } from "./BaseMessage";
import { z } from "zod";

export const ErrorMessageSchema = BaseMessageSchema.extend({
    type: z.literal("error"),
    code: z.string(),
    message: z.string(),
    details: z.unknown().optional()
});
export type ErrorMessage = z.infer<typeof ErrorMessageSchema>;
