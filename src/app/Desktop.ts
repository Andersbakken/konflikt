import type { DesktopSchema } from "./DesktopSchema";
import type { z } from "zod";

export type Desktop = z.infer<typeof DesktopSchema>;
