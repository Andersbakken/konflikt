import { consoleLevel } from "./consoleLevel";
import { format } from "util";
import { homedir } from "os";
import fs from "fs";
import path from "path";
import type { LogLevel } from "./LogLevel";

// Global logger state
let logFile: fs.WriteStream | undefined;
let consolePromptHandler: (() => void) | undefined;
let logBroadcaster: ((level: "verbose" | "debug" | "log" | "error", message: string) => void) | undefined;

// Initialize default log file
const logDir = path.join(homedir(), ".config", "Konflikt");
if (fs.existsSync(logDir)) {
    const logPath = path.join(logDir, "konflikt.log");
    try {
        logFile = fs.createWriteStream(logPath, { flags: "a" });
    } catch {
        // Failed to create log file, continue without it
    }
}

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
    if (logFile) {
        const formatted = formatMessage(levelName, args);
        logFile.write(formatted + "\n");
    }

    // Write to console if level is sufficient
    if (level >= consoleLevel) {
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
    if (logBroadcaster) {
        const formatted = formatMessage(levelName, args);
        const broadcastLevel = levelName.toLowerCase() as "verbose" | "debug" | "log" | "error";
        logBroadcaster(broadcastLevel, formatted);
    }
}
