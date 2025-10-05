import type { ConsolePongMessageSchema } from "./ConsolePongMessageSchema";
import type { z } from "zod";

export type ConsolePongMessage = z.infer<typeof ConsolePongMessageSchema>;
