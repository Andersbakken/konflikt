import { ConsoleCommandMessageSchema } from "./ConsoleCommandMessageSchema";
import { ConsoleErrorMessageSchema } from "./ConsoleErrorMessageSchema";
import { ConsoleLogMessageSchema } from "./ConsoleLogMessageSchema";
import { ConsolePongMessageSchema } from "./ConsolePongMessageSchema";
import { ConsoleResponseMessageSchema } from "./ConsoleResponseMessageSchema";
import { z } from "zod";

export const ConsoleMessageSchema = z.discriminatedUnion("type", [
    ConsoleCommandMessageSchema,
    ConsoleResponseMessageSchema,
    ConsoleErrorMessageSchema,
    ConsoleLogMessageSchema,
    ConsolePongMessageSchema
]);
