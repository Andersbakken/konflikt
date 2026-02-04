import type { MouseButtonSchema } from "./MouseButtonSchema";
import type { z } from "zod";

export type MouseButton = z.infer<typeof MouseButtonSchema>;
