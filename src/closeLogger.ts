import { getLogFile } from "./setLogFile";

export function closeLogger(): void {
    const logFile = getLogFile();
    if (logFile) {
        logFile.end();
    }
}