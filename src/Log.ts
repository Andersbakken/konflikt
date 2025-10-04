import { format } from "util";
import { homedir } from "os";
import fs from "fs";
import path from "path";

export const enum LogLevel {
    Verbose = 0,
    Debug = 1,
    Log = 2,
    Error = 3,
    Silent = 4
}

// Global logger state
let logFile: fs.WriteStream | undefined;
let consoleLevel: LogLevel = LogLevel.Log;

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

export function setConsoleLevel(level: LogLevel): void {
    consoleLevel = level;
}

export function setLogFile(filePath: string): void {
    if (logFile) {
        logFile.end();
    }
    try {
        logFile = fs.createWriteStream(filePath, { flags: "a" });
    } catch (e) {
        console.error(`Failed to open log file: ${filePath}`, e);
    }
}

function formatMessage(level: string, args: unknown[]): string {
    const timestamp = new Date().toISOString();
    const message = format(...args);
    return `[${timestamp}] [${level}] ${message}`;
}

function doLog(
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
        consoleMethod(...args);
    }
}

export function verbose(...args: unknown[]): void {
    doLog(LogLevel.Verbose, "VERBOSE", console.log, ...args);
}

export function debug(...args: unknown[]): void {
    doLog(LogLevel.Debug, "DEBUG", console.log, ...args);
}

export function log(...args: unknown[]): void {
    doLog(LogLevel.Log, "INFO", console.log, ...args);
}

export function error(...args: unknown[]): void {
    doLog(LogLevel.Error, "ERROR", console.error, ...args);
}

export function closeLogger(): void {
    if (logFile) {
        logFile.end();
    }
}

// Native logging interface - provides callbacks for C++ code
export interface NativeLogger {
    verbose(message: string): void;
    debug(message: string): void;
    log(message: string): void;
    error(message: string): void;
}

// Create logger instance for native code
export function createNativeLogger(): NativeLogger {
    return {
        verbose: (message: string): void => {
            verbose(message);
        },
        debug: (message: string): void => {
            debug(message);
        },
        log: (message: string): void => {
            log(message);
        },
        error: (message: string): void => {
            error(message);
        }
    };
}
