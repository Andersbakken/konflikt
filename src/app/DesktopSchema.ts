import { z } from "zod";

export const DesktopSchema = z.object({
    width: z.number(),
    height: z.number()
});
