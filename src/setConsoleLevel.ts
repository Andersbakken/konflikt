import type { LogLevel } from "./LogLevel";

let consoleLevel: LogLevel = 2; // LogLevel.Log

export function setConsoleLevel(level: LogLevel): void {
    consoleLevel = level;
}

export function getConsoleLevel(): LogLevel {
    return consoleLevel;
}