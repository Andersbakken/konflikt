import { z } from "zod";

// Re-export all message types and schemas
export * from "./BaseMessage";
export * from "./HandshakeRequest";
export * from "./HandshakeResponse";
export * from "./Heartbeat";
export * from "./Disconnect";
export * from "./ErrorMessage";
export * from "./MouseButton";
export * from "./KeyboardModifier";
export * from "./State";
export * from "./Desktop";
export * from "./MouseMoveEvent";
export * from "./MousePressEvent";
export * from "./MouseReleaseEvent";
export * from "./KeyPressEvent";
export * from "./KeyReleaseEvent";

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
import type { BaseMessage } from "./BaseMessage";
import type { ErrorMessage } from "./ErrorMessage";

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
    KeyReleaseEventSchema,
]);

export type Message = z.infer<typeof MessageSchema>;

// Helper functions
export function createBaseMessage(source: string): BaseMessage {
    return {
        id: crypto.randomUUID(),
        timestamp: Date.now(),
        source,
    };
}

export function validateMessage(data: unknown): Message | null {
    try {
        return MessageSchema.parse(data);
    } catch {
        return null;
    }
}

export function createErrorMessage(source: string, code: string, message: string, details?: unknown): ErrorMessage {
    return {
        ...createBaseMessage(source),
        type: "error",
        code,
        message,
        details,
    };
}