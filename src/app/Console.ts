import { createInterface } from "readline";
import { setConsolePromptHandler } from "./consolePromptHandler";
import type { Config } from "./Config";
import type { Konflikt } from "./Konflikt";

export class Console {
    #readline: ReturnType<typeof createInterface>;
    #konflikt: Konflikt;
    #commands: Map<string, ConsoleCommand>;
    #closed = false;

    constructor(konflikt: Konflikt) {
        this.#konflikt = konflikt;

        // Check if stdin is available and connected to a TTY
        if (!process.stdin.readable || !process.stdin.isTTY) {
            throw new Error("Console requires an interactive TTY stdin");
        }

        this.#readline = createInterface({
            input: process.stdin,
            output: process.stdout,
            prompt: "konflikt> "
        });

        this.#commands = new Map();
        this.#registerCommands();
        this.#setupEventHandlers();
    }

    start(): void {
        // Register this console's prompt handler with the logger
        setConsolePromptHandler(() => {
            this.#readline.prompt(true);
        });

        // Clear any existing output and show console startup message
        process.stdout.write('\n');
        this.#consoleLog("Interactive console started. Type 'help' for available commands.");
    }

    stop(): void {
        // Unregister the prompt handler
        setConsolePromptHandler(undefined);
        this.#closed = true;
        this.#readline.close();
    }

    // Private method for console-specific logging that doesn't interfere with debug output
    #consoleLog(...args: unknown[]): void {
        // Clear the current line and write our message
        process.stdout.clearLine(0);
        process.stdout.cursorTo(0);
        console.log(...args);
        // Restore the prompt (but not if we're closed)
        if (!this.#closed) {
            this.#readline.prompt(true);
        }
    }

    #setupEventHandlers(): void {
        this.#readline.on("line", (input: string) => {
            const trimmed = input.trim();
            if (trimmed) {
                this.#handleCommand(trimmed);
            }
            this.#readline.prompt();
        });

        this.#readline.on("close", () => {
            this.#closed = true;
            console.log("\nConsole closed");
            process.exit(0);
        });

        // Handle stdin being closed externally
        process.stdin.on("end", () => {
            this.#closed = true;
            console.log("\nStdin closed, shutting down...");
            process.exit(0);
        });

        process.stdin.on("error", (err: Error) => {
            this.#closed = true;
            console.log(`\nStdin error: ${err.message}`);
            process.exit(1);
        });

        // Handle process signals gracefully
        process.on("SIGPIPE", () => {
            this.#closed = true;
            console.log("\nBroken pipe, shutting down...");
            process.exit(0);
        });
    }

    #handleCommand(input: string): void {
        const parts = input.split(" ");
        const commandName = parts[0];
        if (!commandName) {
            return;
        }

        const args = parts.slice(1);
        const command = this.#commands.get(commandName);
        if (command) {
            try {
                command.handler(args);
            } catch (err) {
                this.#consoleLog(`Error executing command '${commandName}':`, err);
            }
        } else {
            this.#consoleLog(`Unknown command: ${commandName}. Type 'help' for available commands.`);
        }
    }

    #registerCommands(): void {
        this.#commands.set("help", {
            description: "Show available commands",
            usage: "help [command]",
            handler: (args: string[]) => {
                if (args.length > 0) {
                    const commandName = args[0];
                    if (commandName) {
                        const command = this.#commands.get(commandName);
                        if (command) {
                            this.#consoleLog(`${commandName}: ${command.description}`);
                            this.#consoleLog(`Usage: ${command.usage}`);
                        } else {
                            this.#consoleLog(`Unknown command: ${commandName}`);
                        }
                    }
                } else {
                    this.#consoleLog("Available commands:");
                    for (const [name, cmd] of this.#commands) {
                        this.#consoleLog(`  ${name.padEnd(15)} - ${cmd.description}`);
                    }
                    this.#consoleLog("\\nType 'help <command>' for detailed usage.");
                }
            }
        });

        this.#commands.set("config", {
            description: "Show configuration information",
            usage: "config [key]",
            handler: (args: string[]) => {
                const config = this.#konflikt.config;
                if (args.length > 0) {
                    const key = args[0];
                    if (key) {
                        this.#showSpecificConfig(config, key);
                    }
                } else {
                    this.#showAllConfig(config);
                }
            }
        });

        this.#commands.set("connections", {
            description: "Show WebSocket connection information",
            usage: "connections",
            handler: () => {
                this.#showConnections();
            }
        });

        this.#commands.set("server", {
            description: "Show server status and information",
            usage: "server",
            handler: () => {
                this.#showServerStatus();
            }
        });

        this.#commands.set("discovery", {
            description: "Show service discovery information",
            usage: "discovery",
            handler: () => {
                this.#showDiscoveryInfo();
            }
        });

        this.#commands.set("status", {
            description: "Show overall system status",
            usage: "status",
            handler: () => {
                this.#showSystemStatus();
            }
        });

        this.#commands.set("screen", {
            description: "Show screen configuration",
            usage: "screen",
            handler: () => {
                this.#showScreenConfig();
            }
        });

        this.#commands.set("input", {
            description: "Show input configuration and status",
            usage: "input",
            handler: () => {
                this.#showInputConfig();
            }
        });

        this.#commands.set("peers", {
            description: "Show configured peers and adjacency",
            usage: "peers",
            handler: () => {
                this.#showPeersConfig();
            }
        });

        this.#commands.set("debug", {
            description: "Toggle debug output or show debug info",
            usage: "debug [on|off]",
            handler: (args: string[]) => {
                if (args.length > 0) {
                    const arg = args[0];
                    if (arg) {
                        const setting = arg.toLowerCase();
                        if (setting === "on" || setting === "off") {
                            this.#consoleLog(`Debug output ${setting} (note: log level is controlled by --log-level)`);
                        } else {
                            this.#consoleLog("Usage: debug [on|off]");
                        }
                    }
                } else {
                    this.#showDebugInfo();
                }
            }
        });

        this.#commands.set("exit", {
            description: "Exit the application",
            usage: "exit",
            handler: () => {
                this.#consoleLog("Exiting...");
                process.exit(0);
            }
        });

        this.#commands.set("quit", {
            description: "Exit the application gracefully",
            usage: "quit",
            handler: () => {
                this.#consoleLog("Exiting...");
                this.#konflikt.quit();
            }
        });
    }

    #showAllConfig(config: Config): void {
        this.#consoleLog("Current Configuration:");
        this.#consoleLog("======================");

        this.#consoleLog("\\nInstance:");
        this.#consoleLog(`  ID: ${config.instanceId}`);
        this.#consoleLog(`  Name: ${config.instanceName}`);
        this.#consoleLog(`  Role: ${config.role}`);

        this.#consoleLog("\\nNetwork:");
        this.#consoleLog(`  Port: ${config.port}`);
        this.#consoleLog(`  Host: ${config.host}`);
        this.#consoleLog(`  Discovery Enabled: ${config.discoveryEnabled}`);
        this.#consoleLog(`  Service Name: ${config.serviceName}`);

        this.#consoleLog("\\nLogging:");
        this.#consoleLog(`  Level: ${config.logLevel}`);
        this.#consoleLog(`  File: ${config.logFile || "none"}`);

        this.#consoleLog("\\nDevelopment:");
        this.#consoleLog(`  Enabled: ${config.developmentEnabled}`);
        this.#consoleLog(`  Mock Input: ${config.mockInput}`);
    }

    #showSpecificConfig(config: Config, key: string): void {
        const configMap: Record<string, () => unknown> = {
            "instance.id": () => config.instanceId,
            "instance.name": () => config.instanceName,
            "instance.role": () => config.role,
            "network.port": () => config.port,
            "network.host": () => config.host,
            "network.discovery": () => config.discoveryEnabled,
            "network.serviceName": () => config.serviceName,
            "screen.id": () => config.screenId,
            "screen.x": () => config.screenX,
            "screen.y": () => config.screenY,
            "screen.width": () => config.screenWidth,
            "screen.height": () => config.screenHeight,
            "screen.edges": () => config.screenEdges,
            "cluster.server.host": () => config.serverHost,
            "cluster.server.port": () => config.serverPort,
            "cluster.peers": () => config.peers,
            "cluster.adjacency": () => config.adjacency,
            "input.capture.mouse": () => config.captureMouse,
            "input.capture.keyboard": () => config.captureKeyboard,
            "input.forward": () => config.forwardEvents,
            "input.cursorTransition": () => config.cursorTransitionEnabled,
            "input.deadZone": () => config.deadZone,
            "logging.level": () => config.logLevel,
            "logging.file": () => config.logFile,
            "development.enabled": () => config.developmentEnabled,
            "development.mockInput": () => config.mockInput
        };

        const getter = configMap[key];
        if (getter) {
            const value = getter();
            this.#consoleLog(`${key}: ${JSON.stringify(value, null, 2)}`);
        } else {
            this.#consoleLog(`Unknown config key: ${key}`);
            this.#consoleLog("Available keys:", Object.keys(configMap).join(", "));
        }
    }

    #showConnections(): void {
        // Access WebSocket server through server's internal structure
        // Note: This requires accessing private members, which we'll need to expose
        this.#consoleLog("WebSocket Connections:");
        this.#consoleLog("=======================");
        this.#consoleLog("(Connection details require server API exposure)");
        // TODO: Add method to Server class to expose connection count and details
    }

    #showServerStatus(): void {
        const config = this.#konflikt.config;
        this.#consoleLog("Server Status:");
        this.#consoleLog("==============");
        this.#consoleLog(`Port: ${config.port}`);
        this.#consoleLog(`Host: ${config.host}`);
        this.#consoleLog(`Service Name: ${config.serviceName}`);
        this.#consoleLog("Status: Running");
        // TODO: Add more detailed server metrics
    }

    #showDiscoveryInfo(): void {
        const config = this.#konflikt.config;
        this.#consoleLog("Service Discovery:");
        this.#consoleLog("==================");
        this.#consoleLog(`Enabled: ${config.discoveryEnabled}`);
        this.#consoleLog(`Service Name: ${config.serviceName}`);
        this.#consoleLog(`Advertising on port: ${config.port}`);
        // TODO: Show discovered services through server.serviceDiscovery
    }

    #showSystemStatus(): void {
        const config = this.#konflikt.config;
        this.#consoleLog("System Status:");
        this.#consoleLog("==============");
        this.#consoleLog(`Instance: ${config.instanceName} (${config.instanceId})`);
        this.#consoleLog(`Role: ${config.role}`);
        this.#consoleLog(`Server: Running on ${config.host}:${config.port}`);
        this.#consoleLog(`Discovery: ${config.discoveryEnabled ? "Enabled" : "Disabled"}`);
        this.#consoleLog(`Development Mode: ${config.developmentEnabled ? "Yes" : "No"}`);
        this.#consoleLog(`Process ID: ${process.pid}`);
        this.#consoleLog(`Memory Usage: ${Math.round(process.memoryUsage().heapUsed / 1024 / 1024)}MB`);
        this.#consoleLog(`Uptime: ${Math.round(process.uptime())}s`);
    }

    #showScreenConfig(): void {
        const config = this.#konflikt.config;
        this.#consoleLog("Screen Configuration:");
        this.#consoleLog("====================");
        this.#consoleLog(`ID: ${config.screenId}`);
        this.#consoleLog(`Position: (${config.screenX}, ${config.screenY})`);
        this.#consoleLog(`Dimensions: ${config.screenWidth || "auto"} x ${config.screenHeight || "auto"}`);
        const activeEdges = config.screenEdges 
            ? Object.entries(config.screenEdges)
                .filter(([, enabled]: [string, boolean]) => enabled)
                .map(([edge]: [string, boolean]) => edge)
                .join(", ")
            : "";
        this.#consoleLog(`Active Edges: ${activeEdges || "none"}`);
    }

    #showInputConfig(): void {
        const config = this.#konflikt.config;
        this.#consoleLog("Input Configuration:");
        this.#consoleLog("===================");
        this.#consoleLog(`Mouse Capture: ${config.captureMouse ? "Enabled" : "Disabled"}`);
        this.#consoleLog(`Keyboard Capture: ${config.captureKeyboard ? "Enabled" : "Disabled"}`);
        this.#consoleLog(`Cursor Transition: ${config.cursorTransitionEnabled ? "Enabled" : "Disabled"}`);
        this.#consoleLog(`Dead Zone: ${config.deadZone}px`);
        this.#consoleLog(`Forward Events: ${config.forwardEvents.join(", ")}`);
        this.#consoleLog(`Mock Input: ${config.mockInput ? "Enabled" : "Disabled"}`);
    }

    #showPeersConfig(): void {
        const config = this.#konflikt.config;
        this.#consoleLog("Peers Configuration:");
        this.#consoleLog("===================");

        if (config.serverHost && config.serverPort) {
            this.#consoleLog(`Server: ${config.serverHost}:${config.serverPort}`);
        }

        if (config.peers.length > 0) {
            this.#consoleLog(`Manual Peers: ${JSON.stringify(config.peers, null, 2)}`);
        } else {
            this.#consoleLog("Manual Peers: none");
        }

        const adjacency = config.adjacency;
        if (Object.keys(adjacency).length > 0) {
            this.#consoleLog(`Adjacency Map: ${JSON.stringify(adjacency, null, 2)}`);
        } else {
            this.#consoleLog("Adjacency Map: none");
        }
    }

    #showDebugInfo(): void {
        const config = this.#konflikt.config;
        this.#consoleLog("Debug Information:");
        this.#consoleLog("==================");
        this.#consoleLog(`Log Level: ${config.logLevel}`);
        this.#consoleLog(`Log File: ${config.logFile || "console only"}`);
        this.#consoleLog(`Development Mode: ${config.developmentEnabled}`);
        this.#consoleLog(`Mock Input: ${config.mockInput}`);
        this.#consoleLog(`Node Version: ${process.version}`);
        this.#consoleLog(`Platform: ${process.platform} ${process.arch}`);
        this.#consoleLog(`Working Directory: ${process.cwd()}`);
    }
}

interface ConsoleCommand {
    description: string;
    usage: string;
    handler: (args: string[]) => void;
}
