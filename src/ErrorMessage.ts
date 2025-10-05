import type { ErrorMessageSchema } from "./ErrorMessageSchema";
import type { z } from "zod";

export type ErrorMessage = z.infer<typeof ErrorMessageSchema>;
