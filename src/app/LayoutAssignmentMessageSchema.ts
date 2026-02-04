import { z } from "zod";

export const LayoutAssignmentMessageSchema = z.object({
    type: z.literal("layout_assignment"),
    position: z.object({
        x: z.number(),
        y: z.number()
    }),
    adjacency: z.object({
        left: z.string().optional(),
        right: z.string().optional(),
        top: z.string().optional(),
        bottom: z.string().optional()
    }),
    fullLayout: z.array(
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
    )
});
