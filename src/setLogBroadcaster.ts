let logBroadcaster: ((level: "verbose" | "debug" | "log" | "error", message: string) => void) | undefined;

export function setLogBroadcaster(broadcaster: ((level: "verbose" | "debug" | "log" | "error", message: string) => void) | undefined): void {
    logBroadcaster = broadcaster;
}

export function getLogBroadcaster(): ((level: "verbose" | "debug" | "log" | "error", message: string) => void) | undefined {
    return logBroadcaster;
}