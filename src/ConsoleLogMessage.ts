export interface ConsoleLogMessage {
    type: "console_log";
    level: "verbose" | "debug" | "log" | "error";
    message: string;
    timestamp?: number;
}
