import fs from "fs";
import path from "path";
import { Konflikt } from "./Konflikt";
import { homedir } from "os";

const usage = `Usage: konflikt [options]
Options:
  --help, -h     Show this help message
  --version, -v  Show version number
  --config, -c   Path to config file (default: ~/.config/Konflikt/config.json)
`;

let configPath: string = path.join(homedir(), ".config", "Konflikt", "config.json");
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

try {
    const konflikt = new Konflikt(configPath);
    konflikt.init();
} catch (e: unknown) {
    console.error(usage);
    console.error("Error initializing Konflikt:", e);
    process.exit(1);
}
