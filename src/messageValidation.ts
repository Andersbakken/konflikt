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

// Input event message types for source/target architecture
export interface InputEventData {
    x: number;
    y: number;
    timestamp: number;
    keyboardModifiers: number;
    mouseButtons: number;
    // Additional fields for specific event types
    keycode?: number | undefined;
    text?: string | undefined;
    button?: number | undefined;
}

export interface InputEventMessage {
    type: "input_event";
    sourceInstanceId: string;
    sourceDisplayId: string;
    sourceMachineId: string;
    eventType: "keyPress" | "keyRelease" | "mousePress" | "mouseRelease" | "mouseMove";
    eventData: InputEventData;
}

export interface InstanceInfoMessage {
    type: "instance_info";
    instanceId: string;
    displayId: string;
    machineId: string;
    timestamp: number;
}

export type NetworkMessage = ConsoleMessage | InputEventMessage | InstanceInfoMessage;

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

export function isInstanceInfoMessage(obj: unknown): obj is InstanceInfoMessage {
    if (typeof obj !== "object" || obj === null || !("type" in obj) || obj.type !== "instance_info") {
        return false;
    }

    const msg = obj as Record<string, unknown>;
    return (
        typeof msg.instanceId === "string" &&
        typeof msg.displayId === "string" &&
        typeof msg.machineId === "string" &&
        typeof msg.timestamp === "number"
    );
}

export function isNetworkMessage(obj: unknown): obj is NetworkMessage {
    return isConsoleMessage(obj) || isInputEventMessage(obj) || isInstanceInfoMessage(obj);
}
