import { z } from "zod";

export const DesktopSchema = z.object({
    width: z.number(),
    height: z.number(),
});
export type Desktop = z.infer<typeof DesktopSchema>;