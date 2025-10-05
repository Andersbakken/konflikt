import { type ConfigType, SHORT_OPTIONS, configSchema } from "./ConfigSchema";
import { debug } from "./debug";
import { error } from "./error";
import { existsSync, readFileSync } from "fs";
import { homedir, hostname } from "os";
import { runInNewContext } from "vm";
import path from "path";
import type { ScreenEdges } from "./ScreenEdges";

export class Config {
    #convictConfig: ConvictInstance;

    constructor(configPath?: string, cliArgs: string[] = process.argv.slice(2)) {
        // Create a new instance from the schema
        this.#convictConfig = configSchema as ConvictInstance;
        this.#loadConfig(configPath);
        this.#loadCliArgs(cliArgs);
        this.#generateDefaults();
        this.#validate();
    }

    #get(key: string): unknown {
        return this.#convictConfig.get(key);
    }

    #string(key: string): string | null {
        const value = this.#get(key);
        if (value === null) {
            return null;
        }
        if (typeof value === "string") {
            return value;
        }
        throw new Error(`Config key '${key}' is not a string`);
    }

    #integer(key: string): number | null {
        const value = this.#get(key);
        if (value === null) {
            return null;
        }
        if (typeof value === "number" && Number.isInteger(value)) {
            return value;
        }
        throw new Error(`Config key '${key}' is not an integer`);
    }

    #number(key: string): number | null {
        const value = this.#get(key);
        if (value === null) {
            return null;
        }
        if (typeof value === "number") {
            return value;
        }
        throw new Error(`Config key '${key}' is not a number`);
    }

    #set(key: string, value: unknown): void {
        this.#convictConfig.set(key, value);
    }

    // Typed getters for all configuration values with defaults

    // Instance configuration
    get instanceId(): string {
        return (this.#get("instance.id") as string) || `konflikt-${process.pid}-${Date.now()}`;
    }

    get instanceName(): string {
        return (this.#get("instance.name") as string) || `${hostname()}-${process.pid}`;
    }

    get role(): InstanceRole {
        const role = this.#string("instance.role") || "client";
        return role === "server" ? InstanceRole.Server : InstanceRole.Client;
    }

    // Network configuration
    get port(): number {
        return this.#integer("network.port") || 3000;
    }

    get host(): string {
        return this.#string("network.host") || "0.0.0.0";
    }

    get discoveryEnabled(): boolean {
        const enabled = this.#get("network.discovery.enabled") as boolean | null;
        return enabled ?? true;
    }

    get serviceName(): string {
        return this.#string("network.discovery.serviceName") || "konflikt";
    }

    // Screen configuration
    get screenId(): string {
        return this.#string("screen.id") || `screen-${this.instanceId}`;
    }

    get screenX(): number {
        return this.#number("screen.position.x") || 0;
    }

    get screenY(): number {
        return this.#number("screen.position.y") || 0;
    }

    get screenWidth(): number | null {
        return this.#integer("screen.dimensions.width");
    }

    get screenHeight(): number | null {
        return this.#integer("screen.dimensions.height");
    }

    get screenEdges(): ScreenEdges {
        const edges = this.#get("screen.edges") as {
            top: boolean;
            right: boolean;
            bottom: boolean;
            left: boolean;
        } | null;
        return edges || { top: true, right: true, bottom: true, left: true };
    }

    // Cluster configuration

    get serverHost(): string | null {
        return this.#string("cluster.server.host");
    }

    get serverPort(): number | null {
        return this.#integer("cluster.server.port");
    }

    get peers(): unknown[] {
        const peers = this.#get("cluster.peers") as unknown[] | null;
        return peers || [];
    }

    get adjacency(): Record<string, unknown> {
        const adjacency = this.#get("cluster.adjacency") as Record<string, unknown> | null;
        return adjacency || {};
    }

    // Input configuration
    get captureMouse(): boolean {
        const capture = this.#get("input.capture.mouse") as boolean | null;
        return capture ?? true;
    }

    get captureKeyboard(): boolean {
        const capture = this.#get("input.capture.keyboard") as boolean | null;
        return capture ?? true;
    }

    get forwardEvents(): string[] {
        const events = this.#get("input.forward") as string[] | null;
        return events || ["mouse_move", "mouse_press", "mouse_release", "key_press", "key_release"];
    }

    get cursorTransitionEnabled(): boolean {
        const enabled = this.#get("input.cursorTransition.enabled") as boolean | null;
        return enabled ?? true;
    }

    get deadZone(): number {
        return this.#number("input.cursorTransition.deadZone") || 5;
    }

    // Logging configuration
    get logLevel(): "silent" | "error" | "log" | "debug" | "verbose" {
        const level = this.#get("logging.level") as "silent" | "error" | "log" | "debug" | "verbose" | null;
        return level || "log";
    }

    get logFile(): string | null {
        return this.#string("logging.file");
    }

    // Development configuration
    get developmentEnabled(): boolean {
        const enabled = this.#get("development.enabled") as boolean | null;
        return enabled ?? false;
    }

    get mockInput(): boolean {
        const mock = this.#get("development.mockInput") as boolean | null;
        return mock ?? false;
    }

    // Console configuration
    get console(): boolean | string {
        const value = this.#string("console.enabled");
        if (value === "false") {
            return false;
        }
        if (value === "true" || value === null) {
            return true;
        }
        return value; // host:port string
    }

    getAll(): ConfigType {
        return this.#convictConfig.getProperties();
    }

    // Get the raw convict instance for advanced usage
    getConvictInstance(): ConvictInstance {
        return this.#convictConfig;
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

    #loadConfig(configPath?: string): void {
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
                configData = Config.#executeJsConfig(configContent, configPath);
            } else if (ext === ".json") {
                // Parse JSON config
                configData = JSON.parse(configContent);
            } else {
                // Try to parse as JSON by default
                configData = JSON.parse(configContent);
            }

            if (configData && typeof configData === "object") {
                this.#convictConfig.load(configData as Record<string, unknown>);
                debug(`Loaded config from: ${configPath}`);
            } else {
                error(`Config file must export an object: ${configPath}`);
            }
        } catch (err) {
            error(`Failed to load config file: ${configPath}`, err);
            throw new Error(`Config loading failed: ${err}`);
        }
    }

    #loadCliArgs(args: string[]): void {
        if (args.length === 0) {
            return;
        }

        // Expand short options to long options
        const expandedArgs = Config.#expandShortOptions(args);

        // Debug: log what args we're passing to convict
        debug("Loading CLI args:", expandedArgs.join(" "));

        // Try convict's built-in CLI parsing first
        this.#convictConfig.load({}, { args: expandedArgs });

        // Manual override for important args that might not work with convict
        this.#manuallyParseImportantArgs(expandedArgs);
    }

    #manuallyParseImportantArgs(args: string[]): void {
        let verboseCount = 0;

        for (let i = 0; i < args.length; i++) {
            const arg = args[i] as string | undefined;

            if (!arg) {
                continue;
            }

            // Count verbose flags first
            if (arg === "-v" || arg === "--verbose") {
                verboseCount++;
                continue;
            }

            // Handle --arg=value syntax
            if (arg.includes("=")) {
                const [argName, argValue] = arg.split("=", 2);
                this.#handleArgument(argName as string, argValue as string);
                continue;
            }

            // Handle --arg value syntax
            const value = args[i + 1];

            // Skip if the next arg looks like another option (except for flags)
            if (value?.startsWith("-") && !Config.#isFlagArgument(arg)) {
                continue;
            }

            this.#handleArgument(arg, value);
            if (!Config.#isFlagArgument(arg) && value) {
                ++i; // Skip next value since we consumed it
            }
        }

        // Apply verbose count after processing all args
        if (verboseCount > 0) {
            this.#set("logging.level", "verbose");
            debug(`CLI override: ${verboseCount} verbose flag${verboseCount > 1 ? "s" : ""} - log-level = verbose`);
        }
    }

    #handleArgument(arg: string, value?: string): void {
        switch (arg) {
            case "--port":
                if (value) {
                    this.#set("network.port", parseInt(value, 10));
                    debug(`CLI override: port = ${parseInt(value, 10)}`);
                }
                break;
            case "--host":
                if (value) {
                    this.#set("network.host", value);
                    debug(`CLI override: host = ${value}`);
                }
                break;
            case "--role":
                if (value) {
                    this.#set("instance.role", value);
                    debug(`CLI override: role = ${value}`);
                }
                break;
            case "--log-level":
                if (value) {
                    this.#set("logging.level", value);
                    debug(`CLI override: log-level = ${value}`);
                }
                break;
            case "--log-file":
                if (value) {
                    this.#set("logging.file", value);
                    debug(`CLI override: log-file = ${value}`);
                }
                break;
            case "--dev":
                this.#set("development.enabled", true);
                debug(`CLI override: dev = true`);
                break;
            case "--console":
                if (value && value !== "true") {
                    this.#set("console.enabled", value);
                    debug(`CLI override: console = ${value}`);
                } else {
                    this.#set("console.enabled", "true");
                    debug(`CLI override: console = true`);
                }
                break;
            case "--no-console":
                this.#set("console.enabled", "false");
                debug(`CLI override: no-console = true`);
                break;
            default:
                // Ignore unknown arguments
                break;
        }
    }

    #validate(): void {
        try {
            this.#convictConfig.validate({ allowed: "strict" });
        } catch (err) {
            error("Config validation failed:", err);
            throw err;
        }
    }

    #generateDefaults(): void {
        // Generate instance ID if not provided
        const instanceId = this.#get("instance.id");
        if (!instanceId) {
            this.#set("instance.id", `konflikt-${process.pid}-${Date.now()}`);
        }

        // Generate instance name if not provided
        const instanceName = this.#get("instance.name");
        if (!instanceName) {
            this.#set("instance.name", `${hostname()}-${process.pid}`);
        }

        // Generate screen ID if not provided
        const screenId = this.#get("screen.id");
        if (!screenId) {
            const finalInstanceId = this.#get("instance.id");
            this.#set("screen.id", `screen-${finalInstanceId}`);
        }
    }

    static #isFlagArgument(arg: string): boolean {
        return ["--dev", "--no-console", "-v", "--verbose"].includes(arg);
    }

    static #expandShortOptions(args: string[]): string[] {
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

    static #executeJsConfig(jsCode: string, configPath: string): unknown {
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
