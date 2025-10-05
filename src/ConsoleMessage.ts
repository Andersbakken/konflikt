import type { ConsoleMessageSchema } from "./ConsoleMessageSchema";
import type { z } from "zod";

export type ConsoleMessage = z.infer<typeof ConsoleMessageSchema>;
