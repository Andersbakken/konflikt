import { LogLevel } from "./LogLevel";
import { doLog } from "./doLog";

export function log(...args: unknown[]): void {
    doLog(LogLevel.Log, "INFO", console.log, ...args);
}
