import type { DisconnectSchema } from "./DisconnectSchema";
import type { z } from "zod";

export type Disconnect = z.infer<typeof DisconnectSchema>;
