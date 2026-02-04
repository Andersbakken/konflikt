import type { LayoutAssignmentMessage } from "./LayoutAssignmentMessage";

export function isLayoutAssignmentMessage(obj: unknown): obj is LayoutAssignmentMessage {
    if (typeof obj !== "object" || obj === null || !("type" in obj) || obj.type !== "layout_assignment") {
        return false;
    }

    const msg = obj as Record<string, unknown>;
    return (
        typeof msg.position === "object" &&
        msg.position !== null &&
        typeof (msg.position as Record<string, unknown>).x === "number" &&
        typeof (msg.position as Record<string, unknown>).y === "number" &&
        typeof msg.adjacency === "object" &&
        msg.adjacency !== null &&
        Array.isArray(msg.fullLayout)
    );
}
