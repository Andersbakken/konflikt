#!/usr/bin/env node

import { Config } from "./Config";
import { LogLevel } from "./LogLevel";
import { main } from "./main";
import { setConsoleLevel } from "./consoleLevel";
import { setLogFile } from "./logFile";
import { startRemoteConsole } from "./startRemoteConsole";

if (process.argv.includes("--help") || process.argv.includes("-h")) {
    console.log(`Usage: konflikt [options]
Options:
  --help, -h                Show this help message
  --version                 Show version number
  --config, -c              Path to config file
  --log-level, -l           Log level (silent, error, log, debug, verbose)
  --verbose, -v             Enable verbose logging
  --log-file, -f            Path to log file
  --port, -p                WebSocket server port
  --host, -H                Host to bind server to
  --role, -r                Instance role (server, client)
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
  --dev, -d                 Enable development mode
  --mock-input              Mock input events for testing
  --console                 Enable console (default), or connect to remote host[:port]
  --no-console              Disable console interface
`);
    process.exit(0);
}

if (process.argv.includes("--version")) {
    console.log("Konflikt version 0.1.0");
    process.exit(0);
}

let config: Config;
try {
    config = Config.findAndLoadConfig();
} catch (err) {
    console.error("Failed to load configuration:", err);
    process.exit(1);
}

const consoleConfig = config.console;

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

(async (): Promise<void> => {
    try {
        if (typeof consoleConfig === "string" && consoleConfig !== "true") {
            await startRemoteConsole(consoleConfig, verbosityLevel);
        } else {
            const logFile = config.logFile;
            if (logFile) {
                setLogFile(logFile);
            }
            await main(config);
        }
        process.exit();
    } catch (err: unknown) {
        console.error("Fatal error:", err);
        process.exit(1);
    }
})();
