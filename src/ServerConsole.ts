import { type ConsoleCommandMessage, isConsoleCommandMessage } from "./messageValidation";
import { debug, error } from "./Log";
import type { Config } from "./Config";
import type { IncomingMessage } from "http";
import WebSocket from "ws";

export class ServerConsole {
    #config: Config | undefined;
    #instanceId: string;
    #instanceName: string;
    #version: string;
    #capabilities: string[];
    #port: number;
    #consoleSockets: Set<WebSocket> = new Set();

    constructor(
        port: number,
        instanceId: string,
        instanceName: string,
        version: string = "1.0.0",
        capabilities: string[] = ["input_events", "state_sync"]
    ) {
        this.#port = port;
        this.#instanceId = instanceId;
        this.#instanceName = instanceName;
        this.#version = version;
        this.#capabilities = capabilities;
    }

    setConfig(config: Config): void {
        this.#config = config;
    }

    handleConsoleConnection(socket: WebSocket, req: IncomingMessage, wss: WebSocket.WebSocketServer): void {
        debug(`Console WebSocket connection from ${req.socket.remoteAddress}`);

        // Add socket to our tracking set
        this.#consoleSockets.add(socket);

        socket.on("message", (text: WebSocket.RawData) => {
            if (typeof text !== "string") {
                ServerConsole.#sendConsoleError(socket, "Invalid message format: expected text");
                return;
            }

            try {
                const parsed = JSON.parse(text);

                if (!isConsoleCommandMessage(parsed)) {
                    ServerConsole.#sendConsoleError(socket, "Invalid message format");
                    return;
                }

                this.#handleConsoleCommand(socket, parsed, wss);
            } catch {
                ServerConsole.#sendConsoleError(socket, "Invalid JSON message");
            }
        });

        socket.on("close", () => {
            debug("Console WebSocket connection closed");
            // Remove socket from tracking set
            this.#consoleSockets.delete(socket);
        });

        socket.on("error", (err: Error) => {
            error("Console WebSocket error:", err);
            // Remove socket from tracking set on error
            this.#consoleSockets.delete(socket);
        });

        // Send welcome message
        socket.send(
            JSON.stringify({
                type: "console_response",
                output: `Connected to Konflikt instance: ${this.#instanceName}`
            })
        );
    }

    #handleConsoleCommand(socket: WebSocket, message: ConsoleCommandMessage, wss: WebSocket.WebSocketServer): void {
        const { command, args } = message;

        try {
            let output: string;

            switch (command) {
                case "ping":
                    socket.send(
                        JSON.stringify({
                            type: "pong",
                            timestamp: message.timestamp
                        })
                    );
                    return;

                case "help":
                    output = ServerConsole.#getHelpText();
                    break;

                case "config":
                    output = this.#getConfigInfo(args);
                    break;

                case "status":
                    output = this.#getStatusInfo();
                    break;

                case "server":
                    output = this.#getServerInfo(wss);
                    break;

                case "connections":
                    output = ServerConsole.#getConnectionsInfo(wss);
                    break;

                case "discovery":
                    output = this.#getDiscoveryInfo();
                    break;

                default:
                    ServerConsole.#sendConsoleError(
                        socket,
                        `Unknown command: ${command}. Type 'help' for available commands.`
                    );
                    return;
            }

            ServerConsole.#sendConsoleResponse(socket, output);
        } catch (err: unknown) {
            ServerConsole.#sendConsoleError(socket, `Command execution failed: ${err}`);
        }
    }

    static #sendConsoleError(socket: WebSocket, errorMsg: string): void {
        socket.send(
            JSON.stringify({
                type: "console_error",
                error: errorMsg
            })
        );
    }

    static #getHelpText(): string {
        return `Available commands:
===================
help              - Show this help message
config [key]      - Show configuration information
status            - Show system status
server            - Show server information
connections       - Show WebSocket connections
discovery         - Show service discovery information
ping              - Test connection (returns pong)

Local commands:
disconnect        - Disconnect from remote console`;
    }

    #getConfigInfo(args: string[]): string {
        if (!this.#config) {
            return "Configuration not available";
        }

        if (args.length > 0) {
            const key = args[0];
            if (key) {
                return this.#getSpecificConfigInfo(key);
            }
        }

        return this.#getAllConfigInfo();
    }

    #getAllConfigInfo(): string {
        if (!this.#config) {
            return "Configuration not available";
        }

        return `Current Configuration:
======================

Instance:
  ID: ${this.#config.instanceId}
  Name: ${this.#config.instanceName}
  Role: ${this.#config.role}

Network:
  Port: ${this.#config.port}
  Host: ${this.#config.host}
  Discovery Enabled: ${this.#config.discoveryEnabled}
  Service Name: ${this.#config.serviceName}

Logging:
  Level: ${this.#config.logLevel}
  File: ${this.#config.logFile || "none"}

Development:
  Enabled: ${this.#config.developmentEnabled}
  Mock Input: ${this.#config.mockInput}`;
    }

    #getSpecificConfigInfo(key: string): string {
        if (!this.#config) {
            return "Configuration not available";
        }

        const configMap: Record<string, unknown> = {
            "instance.id": this.#config.instanceId,
            "instance.name": this.#config.instanceName,
            "instance.role": this.#config.role,
            "network.port": this.#config.port,
            "network.host": this.#config.host,
            "network.discovery": this.#config.discoveryEnabled,
            "network.serviceName": this.#config.serviceName,
            "logging.level": this.#config.logLevel,
            "logging.file": this.#config.logFile,
            "development.enabled": this.#config.developmentEnabled,
            "development.mockInput": this.#config.mockInput
        };

        if (key in configMap) {
            return `${key}: ${JSON.stringify(configMap[key], null, 2)}`;
        }

        return `Unknown config key: ${key}
Available keys: ${Object.keys(configMap).join(", ")}`;
    }

    #getStatusInfo(): string {
        const config = this.#config;
        return `System Status:
==============
Instance: ${config?.instanceName || this.#instanceName} (${config?.instanceId || this.#instanceId})
Role: ${config?.role || "unknown"}
Server: Running on ${config?.host || "unknown"}:${this.#port}
Discovery: ${config?.discoveryEnabled ? "Enabled" : "Disabled"}
Development Mode: ${config?.developmentEnabled ? "Yes" : "No"}
Process ID: ${process.pid}
Memory Usage: ${Math.round(process.memoryUsage().heapUsed / 1024 / 1024)}MB
Uptime: ${Math.round(process.uptime())}s`;
    }

    #getServerInfo(wss: WebSocket.WebSocketServer): string {
        return `Server Status:
==============
Port: ${this.#port}
Host: ${this.#config?.host || "unknown"}
Service Name: ${this.#config?.serviceName || "konflikt"}
Status: Running
WebSocket Connections: ${wss.clients.size}
Version: ${this.#version}
Capabilities: ${this.#capabilities.join(", ")}`;
    }

    static #getConnectionsInfo(wss: WebSocket.WebSocketServer): string {
        const clientCount = wss.clients.size;

        return `WebSocket Connections:
=======================
Total Connections: ${clientCount}

Note: Detailed per-connection info requires enhanced tracking`;
    }

    #getDiscoveryInfo(): string {
        const config = this.#config;
        return `Service Discovery:
==================
Enabled: ${config?.discoveryEnabled || "unknown"}
Service Name: ${config?.serviceName || "konflikt"}
Advertising on port: ${this.#port}

Note: Discovered services info requires ServiceDiscovery API exposure`;
    }

    broadcastLogMessage(level: "verbose" | "debug" | "log" | "error", message: string): void {
        const logMessage = {
            type: "console_log",
            level,
            message,
            timestamp: Date.now()
        };

        const messageString = JSON.stringify(logMessage);

        // Send to all connected console sockets
        this.#consoleSockets.forEach((socket: WebSocket) => {
            try {
                if (socket.readyState === WebSocket.OPEN) {
                    socket.send(messageString);
                }
            } catch {
                // Remove broken sockets
                this.#consoleSockets.delete(socket);
            }
        });
    }

    static #sendConsoleResponse(socket: WebSocket, output: string): void {
        socket.send(
            JSON.stringify({
                type: "console_response",
                output
            })
        );
    }
}
