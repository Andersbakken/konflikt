import { LogLevel } from "./LogLevel";

let consoleLevel: LogLevel = LogLevel.Log;

export function setConsoleLevel(level: LogLevel): void {
    consoleLevel = level;
}

export function getConsoleLevel(): LogLevel {
    return consoleLevel;
}
