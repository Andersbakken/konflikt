import { LogLevel } from "./LogLevel";
import { doLog } from "./doLog";

export function error(...args: unknown[]): void {
    doLog(LogLevel.Error, "ERROR", console.error, ...args);
}
