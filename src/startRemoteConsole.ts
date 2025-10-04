import { RemoteConsole, parseRemoteConsoleHost } from "./RemoteConsole";
import { error } from "./Log";

export async function startRemoteConsole(remoteConsoleHost: string): Promise<void> {
    try {
        const { host, port } = parseRemoteConsoleHost(remoteConsoleHost);
        const remoteConsole = new RemoteConsole(host, port);

        await remoteConsole.connect();
        remoteConsole.start();

        // Keep the process running
        process.on("SIGINT", () => {
            remoteConsole.stop();
        });

        process.on("SIGTERM", () => {
            remoteConsole.stop();
        });
    } catch (err) {
        error("Failed to connect to remote console:", err);
        process.exit(1);
    }
}
