import type { MousePressEventSchema } from "./MousePressEventSchema";
import type { z } from "zod";

export type MousePressEvent = z.infer<typeof MousePressEventSchema>;
