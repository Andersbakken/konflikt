import { z } from "zod";

// Import schemas for union type
import { DisconnectSchema } from "./DisconnectSchema";
import { ErrorMessageSchema } from "./ErrorMessageSchema";
import { HandshakeRequestSchema } from "./HandshakeRequestSchema";
import { HandshakeResponseSchema } from "./HandshakeResponseSchema";
import { HeartbeatSchema } from "./HeartbeatSchema";
import { InputEventMessageSchema } from "./InputEventMessageSchema";
import { KeyPressEventSchema } from "./KeyPressEventSchema";
import { KeyReleaseEventSchema } from "./KeyReleaseEventSchema";
import { MouseMoveEventSchema } from "./MouseMoveEventSchema";
import { MousePressEventSchema } from "./MousePressEventSchema";
import { MouseReleaseEventSchema } from "./MouseReleaseEventSchema";

// Union of all message types
export const MessageSchema = z.discriminatedUnion("type", [
    HandshakeRequestSchema,
    HandshakeResponseSchema,
    HeartbeatSchema,
    DisconnectSchema,
    ErrorMessageSchema,
    InputEventMessageSchema,
    MouseMoveEventSchema,
    MousePressEventSchema,
    MouseReleaseEventSchema,
    KeyPressEventSchema,
    KeyReleaseEventSchema
]);
