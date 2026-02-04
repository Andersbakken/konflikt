import type { ConsoleCommandMessageSchema } from "./ConsoleCommandMessageSchema";
import type { z } from "zod";

export type ConsoleCommandMessage = z.infer<typeof ConsoleCommandMessageSchema>;
