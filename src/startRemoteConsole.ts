import { LogLevel } from "./LogLevel";
import { RemoteConsole } from "./RemoteConsole";
import { createPromise } from "./createPromise";
import { discoverKonfliktInstances } from "./discoverKonfliktInstances";
import { log } from "./log";
import { parseRemoteConsoleHost } from "./parseRemoteConsoleHost";
import { verbose } from "./verbose";
import type { DiscoveredService } from "./DiscoveredService";

export async function startRemoteConsole(remoteConsoleHost: string, logLevel: LogLevel = LogLevel.Log): Promise<void> {
    let host: string;
    let port: number;

    // Check if only a hostname was provided (no port)
    if (!remoteConsoleHost.includes(":")) {
        log(`No port specified for '${remoteConsoleHost}', discovering available Konflikt instances...`);

        const discoveredServices = await discoverKonfliktInstances(5000);
        const serverServices = discoveredServices.filter(
            (service: DiscoveredService) => service.txt?.role === "server"
        );

        if (serverServices.length === 0) {
            throw new Error("No Konflikt server instances found via service discovery");
        }

        // Sort by port number and take the lowest
        serverServices.sort((a: DiscoveredService, b: DiscoveredService) => a.port - b.port);
        const selectedService = serverServices[0] as DiscoveredService;

        host = selectedService.host;
        port = selectedService.port;

        log(
            `Found ${serverServices.length} server instance(s), connecting to ${selectedService.name} at ${host}:${port}`
        );
        if (serverServices.length > 1) {
            verbose(
                `Other available instances: ${serverServices
                    .slice(1)
                    .map((s: DiscoveredService) => `${s.name}:${s.port}`)
                    .join(", ")}`
            );
        }
    } else {
        const parsed = parseRemoteConsoleHost(remoteConsoleHost);
        host = parsed.host;
        port = parsed.port;
        verbose(`Connecting to specified host: ${host}:${port}`);
    }

    const remoteConsole = new RemoteConsole(host, port, logLevel);
    await remoteConsole.connect();
    remoteConsole.start();
    const promiseData = createPromise();
    process.on("SIGINT", () => {
        remoteConsole.stop();
        promiseData.resolve();
    });

    process.on("SIGTERM", () => {
        remoteConsole.stop();
        promiseData.reject(new Error("Process terminated"));
    });
    await promiseData.promise;
}
