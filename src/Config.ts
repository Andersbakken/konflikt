import { type ConfigType, SHORT_OPTIONS, configSchema } from "./ConfigSchema";
import { debug, error } from "./Log";
import { existsSync, readFileSync } from "fs";
import { homedir, hostname } from "os";
import { runInNewContext } from "vm";
import path from "path";
interface ConvictInstance {
    get(key: string): unknown;
    set(key: string, value: unknown): void;
    getProperties(): ConfigType;
    load(obj: Record<string, unknown>, options?: { args?: string[] }): void;
    validate(options?: { allowed?: string }): void;
}

export class Config {
    private convictConfig: ConvictInstance;

    constructor(configPath?: string, cliArgs: string[] = process.argv.slice(2)) {
        // Create a new instance from the schema
        this.convictConfig = configSchema as ConvictInstance;
        this.loadConfig(configPath);
        this.loadCliArgs(cliArgs);
        this.generateDefaults();
        this.validate();
    }

    get(key: string): unknown {
        return this.convictConfig.get(key);
    }

    string(key: string): string | null {
        const value = this.get(key);
        if (value === null) {
            return null;
        }
        if (typeof value === "string") {
            return value;
        }
        throw new Error(`Config key '${key}' is not a string`);
    }

    integer(key: string): number | null {
        const value = this.get(key);
        if (value === null) {
            return null;
        }
        if (typeof value === "number" && Number.isInteger(value)) {
            return value;
        }
        throw new Error(`Config key '${key}' is not an integer`);
    }

    number(key: string): number | null {
        const value = this.get(key);
        if (value === null) {
            return null;
        }
        if (typeof value === "number") {
            return value;
        }
        throw new Error(`Config key '${key}' is not a number`);
    }

    set(key: string, value: unknown): void {
        this.convictConfig.set(key, value);
    }

    getAll(): ConfigType {
        return this.convictConfig.getProperties();
    }

    // Get the raw convict instance for advanced usage
    getConvictInstance(): ConvictInstance {
        return this.convictConfig;
    }

    // Export current configuration as JSON (useful for debugging)
    toJSON(): string {
        return JSON.stringify(this.getAll(), null, 2);
    }

    // Get the computed configuration file paths that would be searched
    static getDefaultConfigPaths(): string[] {
        const homeDir = homedir();
        return [
            path.join(process.cwd(), "konflikt.config.js"),
            path.join(process.cwd(), "konflikt.config.json"),
            path.join(process.cwd(), ".konfliktrc.js"),
            path.join(process.cwd(), ".konfliktrc.json"),
            path.join(process.cwd(), ".konfliktrc"),
            path.join(homeDir, ".config", "konflikt", "config.js"),
            path.join(homeDir, ".config", "konflikt", "config.json"),
            path.join(homeDir, ".konfliktrc.js"),
            path.join(homeDir, ".konfliktrc.json"),
            path.join(homeDir, ".konfliktrc")
        ];
    }

    // Find and load the first available config file
    static findAndLoadConfig(cliArgs?: string[]): Config {
        // First check if --config was specified in CLI args
        if (cliArgs) {
            const configIndex = cliArgs.indexOf("--config");
            const configIndexShort = cliArgs.indexOf("-c");

            if (configIndex >= 0 && configIndex + 1 < cliArgs.length) {
                const configPath = cliArgs[configIndex + 1];
                debug(`Using config file from CLI: ${configPath}`);
                return new Config(configPath, cliArgs);
            }

            if (configIndexShort >= 0 && configIndexShort + 1 < cliArgs.length) {
                const configPath = cliArgs[configIndexShort + 1];
                debug(`Using config file from CLI: ${configPath}`);
                return new Config(configPath, cliArgs);
            }
        }

        // Fall back to searching default paths
        const configPaths = Config.getDefaultConfigPaths();

        for (const configPath of configPaths) {
            if (existsSync(configPath)) {
                debug(`Found config file: ${configPath}`);
                return new Config(configPath, cliArgs);
            }
        }

        debug("No config file found, using defaults");
        return new Config(undefined, cliArgs);
    }

    private loadConfig(configPath?: string): void {
        if (!configPath) {
            return;
        }

        if (!existsSync(configPath)) {
            debug(`Config file not found: ${configPath}`);
            return;
        }

        try {
            const ext = path.extname(configPath).toLowerCase();
            const configContent = readFileSync(configPath, "utf8");

            let configData: unknown;

            if (ext === ".js" || ext === ".mjs") {
                // Execute JavaScript config in sandbox
                configData = Config.executeJsConfig(configContent, configPath);
            } else if (ext === ".json") {
                // Parse JSON config
                configData = JSON.parse(configContent);
            } else {
                // Try to parse as JSON by default
                configData = JSON.parse(configContent);
            }

            if (configData && typeof configData === "object") {
                this.convictConfig.load(configData as Record<string, unknown>);
                debug(`Loaded config from: ${configPath}`);
            } else {
                error(`Config file must export an object: ${configPath}`);
            }
        } catch (err) {
            error(`Failed to load config file: ${configPath}`, err);
            throw new Error(`Config loading failed: ${err}`);
        }
    }

    private loadCliArgs(args: string[]): void {
        if (args.length === 0) {
            return;
        }

        // Expand short options to long options
        const expandedArgs = Config.expandShortOptions(args);

        // Debug: log what args we're passing to convict
        debug("Loading CLI args:", expandedArgs.join(" "));

        // Try convict's built-in CLI parsing first
        this.convictConfig.load({}, { args: expandedArgs });

        // Manual override for important args that might not work with convict
        this.manuallyParseImportantArgs(expandedArgs);
    }

    private manuallyParseImportantArgs(args: string[]): void {
        for (let i = 0; i < args.length - 1; i++) {
            const arg = args[i] as string | undefined;
            const value = args[i + 1];

            // Skip if the next arg looks like another option
            if (value?.startsWith("-")) {
                continue;
            }

            switch (arg) {
                case "--port":
                    if (value) {
                        this.convictConfig.set("network.port", parseInt(value, 10));
                        debug(`CLI override: port = ${parseInt(value, 10)}`);
                    }
                    break;
                case "--host":
                    if (value) {
                        this.convictConfig.set("network.host", value);
                        debug(`CLI override: host = ${value}`);
                    }
                    break;
                case "--role":
                    if (value) {
                        this.convictConfig.set("instance.role", value);
                        debug(`CLI override: role = ${value}`);
                    }
                    break;
                case "--log-level":
                    if (value) {
                        this.convictConfig.set("logging.level", value);
                        debug(`CLI override: log-level = ${value}`);
                    }
                    break;
                case "--log-file":
                    if (value) {
                        this.convictConfig.set("logging.file", value);
                        debug(`CLI override: log-file = ${value}`);
                    }
                    break;
                case "--dev":
                    this.convictConfig.set("development.enabled", true);
                    debug(`CLI override: dev = true`);
                    break;
                case undefined:
                default:
                    // Ignore unknown arguments
                    break;
            }
        }
    }

    private validate(): void {
        try {
            this.convictConfig.validate({ allowed: "strict" });
        } catch (err) {
            error("Config validation failed:", err);
            throw err;
        }
    }

    private generateDefaults(): void {
        // Generate instance ID if not provided
        const instanceId = this.convictConfig.get("instance.id");
        if (!instanceId) {
            this.convictConfig.set("instance.id", `konflikt-${process.pid}-${Date.now()}`);
        }

        // Generate instance name if not provided
        const instanceName = this.convictConfig.get("instance.name");
        if (!instanceName) {
            this.convictConfig.set("instance.name", `${hostname()}-${process.pid}`);
        }

        // Generate screen ID if not provided
        const screenId = this.convictConfig.get("screen.id");
        if (!screenId) {
            const finalInstanceId = this.convictConfig.get("instance.id");
            this.convictConfig.set("screen.id", `screen-${finalInstanceId}`);
        }
    }

    private static expandShortOptions(args: string[]): string[] {
        const expanded: string[] = [];

        for (let i = 0; i < args.length; i++) {
            const arg = args[i];
            if (!arg) {
                continue;
            }

            const shortOption = SHORT_OPTIONS[arg];
            if (shortOption) {
                expanded.push(shortOption);
            } else {
                expanded.push(arg);
            }
        }

        return expanded;
    }

    private static executeJsConfig(jsCode: string, configPath: string): unknown {
        debug(`Executing JavaScript config in sandbox: ${configPath}`);

        // Create a minimal sandbox environment
        const sandbox = {
            module: { exports: {} },
            exports: {},
            require: (id: string): unknown => {
                // Only allow specific safe modules
                const allowedModules = ["os", "path", "crypto"];
                if (allowedModules.includes(id)) {
                    // eslint-disable-next-line @typescript-eslint/no-require-imports
                    return require(id);
                }
                throw new Error(`Module '${id}' is not allowed in config`);
            },
            console: {
                log: debug,
                error: error,
                warn: debug,
                info: debug
            },
            process: {
                env: { ...process.env },
                platform: process.platform,
                arch: process.arch,
                pid: process.pid
            },
            // Utilities that config scripts might need
            __filename: configPath,
            __dirname: path.dirname(configPath)
        };

        try {
            // Execute the config script in the sandbox
            runInNewContext(jsCode, sandbox, {
                filename: configPath,
                timeout: 5000, // 5 second timeout
                displayErrors: true
            });

            // Return the exported configuration
            return (sandbox.module.exports as unknown) || (sandbox.exports as unknown);
        } catch (err) {
            error(`Error executing JavaScript config: ${configPath}`, err);
            throw new Error(`JavaScript config execution failed: ${err}`);
        }
    }
}
