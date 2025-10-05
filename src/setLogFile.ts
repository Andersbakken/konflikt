import { createWriteStream } from "fs";
import type { WriteStream } from "fs";

let logFile: WriteStream | undefined;

export function setLogFile(filePath: string): void {
    logFile?.end();
    logFile = createWriteStream(filePath, { flags: "a" });
}

export function getLogFile(): WriteStream | undefined {
    return logFile;
}