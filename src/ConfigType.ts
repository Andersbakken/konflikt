import type { configSchema } from "./configSchema";

export type ConfigType = ReturnType<typeof configSchema.getProperties>;
