import type { HandshakeRequestSchema } from "./HandshakeRequestSchema";
import type { z } from "zod";

export type HandshakeRequest = z.infer<typeof HandshakeRequestSchema>;
