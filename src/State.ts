import type { StateSchema } from "./StateSchema";
import type { z } from "zod";

export type State = z.infer<typeof StateSchema>;
