import type { MessageSchema } from "./MessageSchema";
import type { z } from "zod";

export type Message = z.infer<typeof MessageSchema>;
