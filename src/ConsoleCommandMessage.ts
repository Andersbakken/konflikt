export interface ConsoleCommandMessage {
    type: "console_command";
    command: string;
    args: string[];
    timestamp?: number;
}
