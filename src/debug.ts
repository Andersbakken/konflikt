import { LogLevel } from "./LogLevel";
import { doLog } from "./doLog";

export function debug(...args: unknown[]): void {
    doLog(LogLevel.Debug, "DEBUG", console.log, ...args);
}
