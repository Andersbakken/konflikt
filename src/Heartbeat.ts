import type { HeartbeatSchema } from "./HeartbeatSchema";
import type { z } from "zod";

export type Heartbeat = z.infer<typeof HeartbeatSchema>;
