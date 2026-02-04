import type { ConsoleResponseMessageSchema } from "./ConsoleResponseMessageSchema";
import type { z } from "zod";

export type ConsoleResponseMessage = z.infer<typeof ConsoleResponseMessageSchema>;
