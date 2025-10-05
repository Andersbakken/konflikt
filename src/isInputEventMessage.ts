import type { InputEventMessage } from "./InputEventMessage";

export function isInputEventMessage(obj: unknown): obj is InputEventMessage {
    if (typeof obj !== "object" || obj === null || !("type" in obj) || obj.type !== "input_event") {
        return false;
    }

    const msg = obj as Record<string, unknown>;
    return (
        typeof msg.sourceInstanceId === "string" &&
        typeof msg.sourceDisplayId === "string" &&
        typeof msg.sourceMachineId === "string" &&
        typeof msg.eventType === "string" &&
        ["keyPress", "keyRelease", "mousePress", "mouseRelease", "mouseMove"].includes(msg.eventType) &&
        typeof msg.eventData === "object" &&
        msg.eventData !== null &&
        typeof (msg.eventData as Record<string, unknown>).x === "number" &&
        typeof (msg.eventData as Record<string, unknown>).y === "number" &&
        typeof (msg.eventData as Record<string, unknown>).timestamp === "number" &&
        typeof (msg.eventData as Record<string, unknown>).keyboardModifiers === "number" &&
        typeof (msg.eventData as Record<string, unknown>).mouseButtons === "number"
    );
}
