import type { LayoutUpdateMessage } from "./LayoutUpdateMessage";

export function isLayoutUpdateMessage(obj: unknown): obj is LayoutUpdateMessage {
    if (typeof obj !== "object" || obj === null || !("type" in obj) || obj.type !== "layout_update") {
        return false;
    }

    const msg = obj as Record<string, unknown>;
    return Array.isArray(msg.screens) && typeof msg.timestamp === "number";
}
