import { LogLevel } from "./LogLevel";
import { RemoteConsole } from "./RemoteConsole";
import { parseRemoteConsoleHost } from "./parseRemoteConsoleHost";

export async function startRemoteConsole(remoteConsoleHost: string, logLevel: LogLevel = LogLevel.Log): Promise<void> {
    const { host, port } = parseRemoteConsoleHost(remoteConsoleHost);
    const remoteConsole = new RemoteConsole(host, port, logLevel);

    await remoteConsole.connect();
    remoteConsole.start();
    return new Promise<void>((resolve: () => void, reject: (err: Error) => void) => {
        process.on("SIGINT", () => {
            remoteConsole.stop();
            resolve();
        });

        process.on("SIGTERM", () => {
            remoteConsole.stop();
            reject(new Error("Process terminated"));
        });
    });
}
