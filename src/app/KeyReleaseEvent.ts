import type { KeyReleaseEventSchema } from "./KeyReleaseEventSchema";
import type { z } from "zod";

export type KeyReleaseEvent = z.infer<typeof KeyReleaseEventSchema>;
