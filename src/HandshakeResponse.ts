import type { HandshakeResponseSchema } from "./HandshakeResponseSchema";
import type { z } from "zod";

export type HandshakeResponse = z.infer<typeof HandshakeResponseSchema>;
