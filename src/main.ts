import { Konflikt } from "./Konflikt";
import { debug } from "./debug";
import { log } from "./log";
import type { Config } from "./Config";

export async function main(config: Config): Promise<void> {
    const konflikt = new Konflikt(config);
    await konflikt.init();

    // Keep the process running
    debug("Konflikt is running. Press Ctrl+C to exit.");

    // Handle graceful shutdown
    process.on("SIGINT", (): void => {
        log("\nShutting down...");
        if (konflikt.console) {
            konflikt.console.stop();
        }
        process.exit(0);
    });

    process.on("SIGTERM", (): void => {
        log("\nShutting down...");
        if (konflikt.console) {
            konflikt.console.stop();
        }
        process.exit(0);
    });

    // Keep the process alive indefinitely
    return new Promise<void>(() => {
        // This promise never resolves, keeping the process running
        // Process will only exit through signal handlers above
    });
}
