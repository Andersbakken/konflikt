import type { ConsoleErrorMessageSchema } from "./ConsoleErrorMessageSchema";
import type { z } from "zod";

export type ConsoleErrorMessage = z.infer<typeof ConsoleErrorMessageSchema>;
