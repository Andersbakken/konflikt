import type { KeyPressEventSchema } from "./KeyPressEventSchema";
import type { z } from "zod";

export type KeyPressEvent = z.infer<typeof KeyPressEventSchema>;
