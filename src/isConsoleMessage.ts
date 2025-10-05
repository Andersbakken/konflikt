import type { ConsoleMessage } from "./ConsoleMessage";

export function isConsoleMessage(obj: unknown): obj is ConsoleMessage {
    if (typeof obj !== "object" || obj === null || !("type" in obj)) {
        return false;
    }

    switch (obj.type) {
        case "console_response":
            return "output" in obj && typeof obj.output === "string";
        case "console_error":
            return "error" in obj && typeof obj.error === "string";
        case "pong":
            return !("timestamp" in obj) || typeof (obj as { timestamp: unknown }).timestamp === "number";
        case "console_log":
            return (
                "level" in obj &&
                typeof obj.level === "string" &&
                ["verbose", "debug", "log", "error"].includes(obj.level) &&
                "message" in obj &&
                typeof obj.message === "string" &&
                (!("timestamp" in obj) || typeof (obj as { timestamp: unknown }).timestamp === "number")
            );
        default:
            return false;
    }
}
