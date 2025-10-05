import { LogLevel } from "./LogLevel";
import { doLog } from "./doLog";

export function verbose(...args: unknown[]): void {
    doLog(LogLevel.Verbose, "VERBOSE", console.log, ...args);
}
