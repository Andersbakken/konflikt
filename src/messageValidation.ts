// Console message types with validation
export interface ConsoleCommandMessage {
    type: "console_command";
    command: string;
    args: string[];
    timestamp?: number;
}

export interface ConsoleResponseMessage {
    type: "console_response";
    output: string;
}

export interface ConsoleErrorMessage {
    type: "console_error";
    error: string;
}

export interface ConsolePongMessage {
    type: "pong";
    timestamp?: number;
}

export interface ConsoleLogMessage {
    type: "console_log";
    level: "verbose" | "debug" | "log" | "error";
    message: string;
    timestamp?: number;
}

export type ConsoleMessage = ConsoleResponseMessage | ConsoleErrorMessage | ConsolePongMessage | ConsoleLogMessage;

// Validation functions
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
            return "level" in obj && 
                   typeof obj.level === "string" && 
                   ["verbose", "debug", "log", "error"].includes(obj.level) &&
                   "message" in obj && 
                   typeof obj.message === "string" &&
                   (!("timestamp" in obj) || typeof (obj as { timestamp: unknown }).timestamp === "number");
        default:
            return false;
    }
}