import type { ConsoleLogMessageSchema } from "./ConsoleLogMessageSchema";
import type { z } from "zod";

export type ConsoleLogMessage = z.infer<typeof ConsoleLogMessageSchema>;
