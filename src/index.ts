import { Konflikt } from "./Konflikt";
import { homedir } from "os";
import fs from "fs";
import path from "path";

const usage = `Usage: konflikt [options]
Options:
  --help, -h     Show this help message
  --version, -v  Show version number
  --config, -c   Path to config file (default: ~/.config/Konflikt/config.json)
`;

let configPath: string | undefined = path.join(homedir(), ".config", "Konflikt", "config.json");
if (!fs.existsSync(configPath)) {
    configPath = undefined;
}

for (let idx = 2; idx < process.argv.length; ++idx) {
    const arg = process.argv[idx];
    if (arg === "--help" || arg === "-h") {
        console.log(usage);
        process.exit(0);
    } else if (arg === "--version" || arg === "-v") {
        console.log("Konflikt version 0.1.0");
        process.exit(0);
    } else if (arg === "--config" || arg === "-c") {
        configPath = process.argv[++idx] ?? "";
        if (!fs.existsSync(configPath)) {
            console.error(usage);
            console.error(`Config file not found: ${configPath}`);
            process.exit(1);
        }
    } else {
        console.error(usage);
        console.error(`Unknown argument: ${arg}`);
        process.exit(1);
    }
}

let konflikt: Konflikt;
try {
    konflikt = new Konflikt(configPath);
    konflikt.init();

    // Keep the process running
    console.log("Konflikt is running. Press Ctrl+C to exit.");

    // Handle graceful shutdown
    process.on("SIGINT", (): void => {
        console.log("\nShutting down...");
        process.exit(0);
    });

    process.on("SIGTERM", (): void => {
        console.log("\nShutting down...");
        process.exit(0);
    });

    // Keep process alive
    setInterval((): void => {
        // Just keep the event loop running
    }, 1000);
} catch (e: unknown) {
    console.error(usage);
    console.error("Error initializing Konflikt:", e);
    process.exit(1);
}
