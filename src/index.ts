#!/usr/bin/env node
import { Config } from "./Config";
import { Konflikt } from "./Konflikt";
import { LogLevel, debug, error, log, setConsoleLevel, setLogFile } from "./Log";

// Handle help and version before config loading
if (process.argv.includes("--help") || process.argv.includes("-h")) {
    console.log(`Usage: konflikt [options]
Options:
  --help, -h                Show this help message
  --version                 Show version number
  --config, -c              Path to config file
  --log-level, -l           Log level (silent, error, log, debug, verbose)
  --log-file, -f            Path to log file
  --port, -p                WebSocket server port
  --host, -H                Host to bind server to
  --role, -r                Instance role (server, client, peer)
  --instance-id             Unique instance identifier
  --instance-name           Human-readable instance name
  --screen-x                Screen X position
  --screen-y                Screen Y position
  --screen-width            Screen width in pixels
  --screen-height           Screen height in pixels
  --server-host             Server host to connect to (client mode)
  --server-port             Server port to connect to (client mode)
  --discovery               Enable service discovery
  --service-name, -s        Service name for mDNS advertising
  --topology, -t            Cluster topology (automatic, star, mesh)
  --dev, -d                 Enable development mode
  --mock-input              Mock input events for testing
`);
    process.exit(0);
}

if (process.argv.includes("--version")) {
    console.log("Konflikt version 0.1.0");
    process.exit(0);
}

// Load configuration with CLI argument overrides
let config: Config;
try {
    config = Config.findAndLoadConfig();
} catch (err) {
    console.error("Failed to load configuration:", err);
    process.exit(1);
}

// Set up logging based on config
const logLevel = config.logLevel;
let verbosityLevel: LogLevel;
switch (logLevel) {
    case "silent":
        verbosityLevel = LogLevel.Silent;
        break;

    case "error":
        verbosityLevel = LogLevel.Error;
        break;

    case "log":
        verbosityLevel = LogLevel.Log;
        break;

    case "debug":
        verbosityLevel = LogLevel.Debug;
        break;

    case "verbose":
        verbosityLevel = LogLevel.Verbose;
        break;

    default:
        verbosityLevel = LogLevel.Log;
        break;
}

setConsoleLevel(verbosityLevel);

// Set log file if configured
const logFile = config.logFile;
if (logFile) {
    setLogFile(logFile);
}

let konflikt: Konflikt;
async function main(): Promise<void> {
    try {
        konflikt = new Konflikt(config);
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
    } catch (e: unknown) {
        error("Error initializing Konflikt:", e);
        process.exit(1);
    }
}

main().catch((e: unknown) => {
    console.error("Fatal error:", e);
    process.exit(1);
});
