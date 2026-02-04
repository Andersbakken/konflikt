import { BaseMessageSchema } from "./BaseMessageSchema";
import { z } from "zod";

export const ErrorMessageSchema = BaseMessageSchema.extend({
    type: z.literal("error"),
    code: z.string(),
    message: z.string(),
    details: z.unknown().optional()
});
