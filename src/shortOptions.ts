import { CommandLineArgs } from "./CommandLineArgs.js";

// Generate short option mappings from CommandLineArgs
export const SHORT_OPTIONS: Record<string, string> = Object.fromEntries(
    Object.entries(CommandLineArgs)
        .filter(([, config]: [string, { path: string; short?: string }]) => config.short)
        .map(([longArg, config]: [string, { path: string; short?: string }]) => [config.short!, `--${longArg}`])
);
