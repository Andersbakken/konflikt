import { format } from "util";
import { getConsoleLevel } from "./consoleLevel";
import { getConsolePromptHandler } from "./consolePromptHandler";
import { getLogBroadcaster } from "./logBroadcaster";
import { getLogFile } from "./logFile";
import type { LogLevel } from "./LogLevel";

function formatMessage(level: string, args: unknown[]): string {
    const timestamp = new Date().toISOString();
    const message = format(...args);
    return `[${timestamp}] [${level}] ${message}`;
}

export function doLog(
    level: LogLevel,
    levelName: string,
    consoleMethod: (...args: unknown[]) => void,
    ...args: unknown[]
): void {
    // Write to log file if available
    const logFile = getLogFile();
    if (logFile) {
        const formatted = formatMessage(levelName, args);
        logFile.write(formatted + "\n");
    }

    // Write to console if level is sufficient
    if (level >= getConsoleLevel()) {
        const consolePromptHandler = getConsolePromptHandler();
        if (consolePromptHandler) {
            // Clear prompt, log message, restore prompt
            process.stdout.clearLine(0);
            process.stdout.cursorTo(0);
            consoleMethod(...args);
            consolePromptHandler();
        } else {
            consoleMethod(...args);
        }
    }

    // Broadcast to remote console connections
    const logBroadcaster = getLogBroadcaster();
    if (logBroadcaster) {
        const formatted = formatMessage(levelName, args);
        const broadcastLevel = levelName.toLowerCase() as "verbose" | "debug" | "log" | "error";
        logBroadcaster(broadcastLevel, formatted);
    }
}
