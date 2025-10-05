import type { MouseReleaseEventSchema } from "./MouseReleaseEventSchema";
import type { z } from "zod";

export type MouseReleaseEvent = z.infer<typeof MouseReleaseEventSchema>;
