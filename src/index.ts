#!/usr/bin/env node
import { Config } from "./Config";
import { LogLevel, setConsoleLevel, setLogFile } from "./Log";
import { main } from "./main";
import { startRemoteConsole } from "./startRemoteConsole";

// Handle help and version before config loading
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

// Load configuration with CLI argument overrides
let config: Config;
try {
    config = Config.findAndLoadConfig();
} catch (err) {
    console.error("Failed to load configuration:", err);
    process.exit(1);
}

// Determine console mode
const consoleConfig = config.console;

// Set up logging based on config first so we can pass it to remote console
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

// Check if we're in remote console mode
if (typeof consoleConfig === "string" && consoleConfig !== "true") {
    startRemoteConsole(consoleConfig, verbosityLevel).catch((e: unknown) => {
        console.error("Fatal error starting remote console:", e);
        process.exit(1);
    });
    // Don't continue with normal server startup - use process.exit here is fine since remote console will keep the process running
} else {
    // Continue with normal server startup

    // Set log file if configured
    const logFile = config.logFile;
    if (logFile) {
        setLogFile(logFile);
    }

    main(config).catch((e: unknown) => {
        console.error("Fatal error:", e);
        process.exit(1);
    });
}
