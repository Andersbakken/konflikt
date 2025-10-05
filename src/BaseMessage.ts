import type { BaseMessageSchema } from "./BaseMessageSchema";
import type { z } from "zod";

export type BaseMessage = z.infer<typeof BaseMessageSchema>;
