import { BaseMessageSchema } from "./BaseMessageSchema";
import { z } from "zod";

const SideSchema = z.enum(["left", "right", "top", "bottom"]);
const AlignmentSchema = z.enum(["start", "center", "end"]);

const ScreenGeometrySchema = z.object({
    x: z.number(),
    y: z.number(),
    width: z.number(),
    height: z.number()
});

const PreferredPositionSchema = z.object({
    side: SideSchema,
    alignment: AlignmentSchema.optional()
});

export const HandshakeRequestSchema = BaseMessageSchema.extend({
    type: z.literal("handshake_request"),
    instanceId: z.string(),
    instanceName: z.string(),
    version: z.string(),
    capabilities: z.array(z.string()),
    screenGeometry: ScreenGeometrySchema.optional(),
    preferredPosition: PreferredPositionSchema.optional()
});
