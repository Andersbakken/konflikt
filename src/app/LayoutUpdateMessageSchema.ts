import { z } from "zod";

export const LayoutUpdateMessageSchema = z.object({
    type: z.literal("layout_update"),
    screens: z.array(
        z.object({
            instanceId: z.string(),
            displayName: z.string(),
            x: z.number(),
            y: z.number(),
            width: z.number(),
            height: z.number(),
            isServer: z.boolean(),
            online: z.boolean()
        })
    ),
    timestamp: z.number()
});
