import type { HostPort } from "./HostPort";

export function parseRemoteConsoleHost(hostString: string): HostPort {
    const defaultPort = 3000;

    if (hostString.includes(":")) {
        const [host, portStr] = hostString.split(":");
        const port = parseInt(portStr as string, 10);

        if (isNaN(port) || port < 1 || port > 65535) {
            throw new Error(`Invalid port number: ${portStr}`);
        }

        return { host: host || "localhost", port };
    }

    return { host: hostString || "localhost", port: defaultPort };
}
