import type { KeyboardModifierSchema } from "./KeyboardModifierSchema";
import type { z } from "zod";

export type KeyboardModifier = z.infer<typeof KeyboardModifierSchema>;
