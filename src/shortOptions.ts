import { CommandLineArgs } from "./CommandLineArgs";

// Generate short option mappings from CommandLineArgs
export const shortOptions: Record<string, string> = Object.fromEntries(
    Object.entries(CommandLineArgs)
        .filter(([, config]: [string, { path: string; short?: string }]) => config.short)
        .map(([longArg, config]: [string, { path: string; short?: string }]) => [config.short!, `--${longArg}`])
);
