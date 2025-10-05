import type { ErrorMessageSchema } from "./ErroMessageSchema";
import type { z } from "zod";

export type ErrorMessage = z.infer<typeof ErrorMessageSchema>;
