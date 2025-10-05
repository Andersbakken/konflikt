export interface ConsoleLogMessage {
    type: "console_log";
    level: string;
    message: string;
    timestamp?: number;
}