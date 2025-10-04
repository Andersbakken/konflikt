#!/usr/bin/env node
import { Konflikt } from "./Konflikt";
import { LogLevel, debug, error, log, setConsoleLevel, setLogFile } from "./Log";
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
    string: ["config", "log-file", "verbose"],
    boolean: ["help", "version", "silent"],
    alias: {
        h: "help",
        c: "config",
        v: "verbose"
    },
    unknown: (arg: string) => {
        if (arg.startsWith("-")) {
            // We can't check verbosityLevel here yet since it's set after parsing
            // So we temporarily store the error and handle it later
            console.error(usage);
            console.error(`Unknown argument: ${arg}`);
            process.exit(1);
        }
        return true;
    }
});

// Count verbose flags - minimist gives us array of empty strings for repeated flags
const verboseCount = Array.isArray(args.verbose) ? args.verbose.length : args.verbose === "" ? 1 : 0;

// Handle verbosity first, before any output
let verbosityLevel: LogLevel = LogLevel.Log;

if (args.silent) {
    verbosityLevel = LogLevel.Silent;
} else {
    if (verboseCount === 1) {
        verbosityLevel = LogLevel.Debug;
    } else if (verboseCount >= 2) {
        verbosityLevel = LogLevel.Verbose;
    }
}

setConsoleLevel(verbosityLevel);

// Handle help
if (args.help) {
    if (verbosityLevel !== LogLevel.Silent) {
        console.log(usage);
    }
    process.exit(0);
}

// Handle version
if (args.version) {
    if (verbosityLevel !== LogLevel.Silent) {
        console.log("Konflikt version 0.1.0");
    }
    process.exit(0);
}

// Reject non-option arguments
if (args._.length > 0) {
    if (verbosityLevel !== LogLevel.Silent) {
        console.error(usage);
        console.error(`Unexpected arguments: ${args._.join(" ")}`);
    }
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
    if (verbosityLevel !== LogLevel.Silent) {
        console.error(usage);
        console.error(`Config file not found: ${configPath}`);
    }
    process.exit(1);
}

// Handle log file
if (args["log-file"]) {
    setLogFile(args["log-file"]);
}


let konflikt: Konflikt;
async function main(): Promise<void> {
    try {
        konflikt = new Konflikt(configPath);
        await konflikt.init();

        // Keep the process running
        debug("Konflikt is running. Press Ctrl+C to exit.");

        // Handle graceful shutdown
        process.on("SIGINT", (): void => {
            log("\nShutting down...");
            process.exit(0);
        });

        process.on("SIGTERM", (): void => {
            log("\nShutting down...");
            process.exit(0);
        });
    } catch (e: unknown) {
        error(usage);
        error("Error initializing Konflikt:", e);
        process.exit(1);
    }
}

main().catch((e: unknown) => {
    console.error("Fatal error:", e);
    process.exit(1);
});
