import { z } from "zod";

// Import schemas for union type
import { ActivateClientMessageSchema } from "./ActivateClientMessageSchema";
import { ClientRegistrationMessageSchema } from "./ClientRegistrationMessageSchema";
import { DisconnectSchema } from "./DisconnectSchema";
import { ErrorMessageSchema } from "./ErrorMessageSchema";
import { HandshakeRequestSchema } from "./HandshakeRequestSchema";
import { HandshakeResponseSchema } from "./HandshakeResponseSchema";
import { HeartbeatSchema } from "./HeartbeatSchema";
import { InputEventMessageSchema } from "./InputEventMessageSchema";
import { InstanceInfoMessageSchema } from "./InstanceInfoMessageSchema";
import { KeyPressEventSchema } from "./KeyPressEventSchema";
import { KeyReleaseEventSchema } from "./KeyReleaseEventSchema";
import { LayoutAssignmentMessageSchema } from "./LayoutAssignmentMessageSchema";
import { LayoutUpdateMessageSchema } from "./LayoutUpdateMessageSchema";
import { MouseMoveEventSchema } from "./MouseMoveEventSchema";
import { MousePressEventSchema } from "./MousePressEventSchema";
import { MouseReleaseEventSchema } from "./MouseReleaseEventSchema";

// Union of all message types
export const MessageSchema = z.discriminatedUnion("type", [
    ActivateClientMessageSchema,
    ClientRegistrationMessageSchema,
    DisconnectSchema,
    ErrorMessageSchema,
    HandshakeRequestSchema,
    HandshakeResponseSchema,
    HeartbeatSchema,
    InputEventMessageSchema,
    InstanceInfoMessageSchema,
    KeyPressEventSchema,
    KeyReleaseEventSchema,
    LayoutAssignmentMessageSchema,
    LayoutUpdateMessageSchema,
    MouseMoveEventSchema,
    MousePressEventSchema,
    MouseReleaseEventSchema
]);
