import type { ConsoleCommandMessage } from "./ConsoleCommandMessage";

export function isConsoleCommandMessage(obj: unknown): obj is ConsoleCommandMessage {
    return (
        typeof obj === "object" &&
        obj !== null &&
        "type" in obj &&
        obj.type === "console_command" &&
        "command" in obj &&
        typeof obj.command === "string" &&
        "args" in obj &&
        Array.isArray(obj.args) &&
        obj.args.every((arg: unknown) => typeof arg === "string") &&
        (!("timestamp" in obj) || typeof obj.timestamp === "number")
    );
}
