import { z } from "zod";

export const InstanceInfoMessageSchema = z.object({
    type: z.literal("instance_info"),
    instanceId: z.string(),
    displayId: z.string(),
    machineId: z.string(),
    timestamp: z.number(),
    screenGeometry: z.object({
        x: z.number(),
        y: z.number(),
        width: z.number(),
        height: z.number()
    })
});
