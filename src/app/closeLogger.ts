import { getLogFile } from "./logFile";

export function closeLogger(): void {
    const logFile = getLogFile();
    if (logFile) {
        logFile.end();
    }
}
