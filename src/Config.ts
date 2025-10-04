import { type ConfigType, configSchema } from "./ConfigSchema";
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
        this.validate();
        this.generateDefaults();
    }

    // Convict accessor methods with support for dot notation
    get(key: string): unknown {
        return this.convictConfig.get(key);
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
        // Convict automatically handles CLI args based on the 'arg' property in schema
        if (args.length > 0) {
            this.convictConfig.load({}, { args });
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
