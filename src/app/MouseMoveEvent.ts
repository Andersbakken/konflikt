import type { MouseMoveEventSchema } from "./MouseMoveEventSchema";
import type { z } from "zod";

export type MouseMoveEvent = z.infer<typeof MouseMoveEventSchema>;
