import type { RawData } from "ws";

export function textFromWebSocketMessage(message: RawData): string {
    if (typeof message === "string") {
        return message;
    }
    if (message instanceof Buffer) {
        return message.toString("utf-8");
    }
    if (Array.isArray(message)) {
        return Buffer.concat(message).toString("utf-8");
    }
    throw new Error("Unsupported WebSocket message format");
}
