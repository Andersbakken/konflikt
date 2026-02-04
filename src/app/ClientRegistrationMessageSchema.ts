import { z } from "zod";

export const ClientRegistrationMessageSchema = z.object({
    type: z.literal("client_registration"),
    instanceId: z.string(),
    displayName: z.string(),
    machineId: z.string(),
    screenWidth: z.number(),
    screenHeight: z.number()
});
