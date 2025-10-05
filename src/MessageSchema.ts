import { z } from "zod";

// Import schemas for union type
import { DisconnectSchema } from "./Disconnect";
import { ErrorMessageSchema } from "./ErrorMessage";
import { HandshakeRequestSchema } from "./HandshakeRequest";
import { HandshakeResponseSchema } from "./HandshakeResponse";
import { HeartbeatSchema } from "./Heartbeat";
import { KeyPressEventSchema } from "./KeyPressEvent";
import { KeyReleaseEventSchema } from "./KeyReleaseEvent";
import { MouseMoveEventSchema } from "./MouseMoveEvent";
import { MousePressEventSchema } from "./MousePressEvent";
import { MouseReleaseEventSchema } from "./MouseReleaseEvent";

// Union of all message types
export const MessageSchema = z.discriminatedUnion("type", [
    HandshakeRequestSchema,
    HandshakeResponseSchema,
    HeartbeatSchema,
    DisconnectSchema,
    ErrorMessageSchema,
    MouseMoveEventSchema,
    MousePressEventSchema,
    MouseReleaseEventSchema,
    KeyPressEventSchema,
    KeyReleaseEventSchema
]);
