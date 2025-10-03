import { Konflikt } from "./Konflikt";
import { LogLevel, setConsoleLevel, setLogFile } from "./Log";
import { homedir } from "os";
import fs from "fs";
import minimist from "minimist";
import path from "path";

const usage = `Usage: konflikt [options]
Options:
  --help, -h       Show this help message
  --version        Show version number
  --config, -c     Path to config file (default: ~/.config/Konflikt/config.json)
  --verbose, -v    Increase verbosity (use -v for debug, -vv for verbose)
  --silent         Suppress all console output
  --log-file       Path to log file
`;

const args = minimist(process.argv.slice(2), {
    string: ["config", "log-file"],
    boolean: ["help", "version", "verbose", "silent"],
    alias: {
        h: "help",
        c: "config",
        v: "verbose"
    },
    unknown: (arg: string) => {
        if (arg.startsWith("-")) {
            console.error(usage);
            console.error(`Unknown argument: ${arg}`);
            process.exit(1);
        }
        return true;
    }
});

// Handle help
if (args.help) {
    console.log(usage);
    process.exit(0);
}

// Handle version
if (args.version) {
    console.log("Konflikt version 0.1.0");
    process.exit(0);
}

// Reject non-option arguments
if (args._.length > 0) {
    console.error(usage);
    console.error(`Unexpected arguments: ${args._.join(" ")}`);
    process.exit(1);
}

// Handle config path
let configPath: string | undefined = args.config;
if (!configPath) {
    configPath = path.join(homedir(), ".config", "Konflikt", "config.json");
    if (!fs.existsSync(configPath)) {
        configPath = undefined;
    }
} else if (!fs.existsSync(configPath)) {
    console.error(usage);
    console.error(`Config file not found: ${configPath}`);
    process.exit(1);
}

// Handle log file
if (args["log-file"]) {
    setLogFile(args["log-file"]);
}

// Handle verbosity
let verbosityLevel: LogLevel = LogLevel.Log;

if (args.silent) {
    verbosityLevel = LogLevel.Silent;
} else {
    // Count how many times -v/--verbose was passed
    const verboseCount = Array.isArray(args.verbose) ? args.verbose.filter(Boolean).length : args.verbose ? 1 : 0;

    if (verboseCount === 1) {
        verbosityLevel = LogLevel.Debug;
    } else if (verboseCount >= 2) {
        verbosityLevel = LogLevel.Verbose;
    }
}

setConsoleLevel(verbosityLevel);

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
} catch (e: unknown) {
    console.error(usage);
    console.error("Error initializing Konflikt:", e);
    process.exit(1);
}
