import { LogLevel } from "./Log";
import { RemoteConsole, parseRemoteConsoleHost } from "./RemoteConsole";

export async function startRemoteConsole(remoteConsoleHost: string, logLevel: LogLevel = LogLevel.Log): Promise<void> {
    const { host, port } = parseRemoteConsoleHost(remoteConsoleHost);
    const remoteConsole = new RemoteConsole(host, port, logLevel);

    await remoteConsole.connect();
    remoteConsole.start();

    // Keep the process running
    process.on("SIGINT", () => {
        remoteConsole.stop();
        process.exit(0);
    });

    process.on("SIGTERM", () => {
        remoteConsole.stop();
        process.exit(0);
    });

    // Keep the process alive indefinitely
    return new Promise<void>(() => {
        // This never resolves, keeping the process alive
    });
}
