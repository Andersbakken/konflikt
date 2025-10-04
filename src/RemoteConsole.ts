import { type ConsoleMessage, isConsoleMessage } from "./types";
import { createInterface } from "readline";
import { debug, error } from "./Log";
import WebSocket from "ws";

export class RemoteConsole {
    #readline: ReturnType<typeof createInterface>;
    #ws: WebSocket | undefined;
    #host: string;
    #port: number;
    #connected: boolean = false;

    constructor(host: string, port: number = 3000) {
        this.#host = host;
        this.#port = port;

        // Check if stdin is available and connected to a TTY
        if (!process.stdin.readable || !process.stdin.isTTY) {
            throw new Error("Remote console requires an interactive TTY stdin");
        }

        this.#readline = createInterface({
            input: process.stdin,
            output: process.stdout,
            prompt: `${host}:${port}> `
        });

        this.#setupEventHandlers();
    }

    async connect(): Promise<void> {
        const wsUrl = `ws://${this.#host}:${this.#port}/console`;
        debug(`Connecting to remote console at ${wsUrl}...`);

        try {
            this.#ws = new WebSocket(wsUrl);

            await new Promise<void>((resolve: () => void, reject: (err: Error) => void) => {
                if (!this.#ws) {
                    reject(new Error("WebSocket not initialized"));
                    return;
                }

                this.#ws.on("open", () => {
                    this.#connected = true;
                    this.#consoleLog(`Connected to Konflikt console at ${this.#host}:${this.#port}`);
                    this.#consoleLog("Type 'help' for available commands, 'disconnect' to exit.");
                    resolve();
                });

                this.#ws.on("error", (err: Error) => {
                    error(`WebSocket connection error: ${err.message}`);
                    reject(err);
                });

                this.#ws.on("close", (code: number, reason: Buffer) => {
                    this.#connected = false;
                    const msg = reason.length > 0 ? reason.toString() : `Connection closed (code: ${code})`;
                    this.#consoleLog(`\\nDisconnected: ${msg}`);
                    process.exit(0);
                });

                this.#ws.on("message", (text: WebSocket.RawData) => {
                    if (typeof text !== "string") {
                        error("Received non-string message from server");
                        return;
                    }
                    try {
                        const parsed = JSON.parse(text);
                        if (!isConsoleMessage(parsed)) {
                            error("Invalid console message format from server");
                            return;
                        }
                        this.#handleServerMessage(parsed);
                    } catch {
                        error("Failed to parse server message");
                    }
                });
            });
        } catch (err: unknown) {
            error(`Failed to connect to ${wsUrl}:`, err);
            throw err;
        }
    }

    start(): void {
        this.#readline.prompt();
    }

    stop(): void {
        if (this.#ws && this.#connected) {
            this.#ws.close();
        }
        this.#readline.close();
    }

    #consoleLog(...args: unknown[]): void {
        // Clear the current line and write our message
        process.stdout.clearLine(0);
        process.stdout.cursorTo(0);
        console.log(...args);
        // Restore the prompt
        this.#readline.prompt(true);
    }

    #setupEventHandlers(): void {
        this.#readline.on("line", (input: string) => {
            const trimmed = input.trim();
            if (trimmed) {
                this.#handleCommand(trimmed);
            } else {
                this.#readline.prompt();
            }
        });

        this.#readline.on("close", () => {
            this.#consoleLog("\\nConsole closed");
            this.stop();
        });

        // Handle stdin being closed externally
        process.stdin.on("end", () => {
            this.#consoleLog("\\nStdin closed, shutting down...");
            this.stop();
        });

        process.stdin.on("error", (err: Error) => {
            this.#consoleLog(`\\nStdin error: ${err.message}`);
            process.exit(1);
        });

        // Handle process signals gracefully
        process.on("SIGPIPE", () => {
            this.#consoleLog("\\nBroken pipe, shutting down...");
            this.stop();
        });

        process.on("SIGINT", () => {
            this.#consoleLog("\\nDisconnecting...");
            this.stop();
        });

        process.on("SIGTERM", () => {
            this.#consoleLog("\\nDisconnecting...");
            this.stop();
        });
    }

    #handleCommand(input: string): void {
        // Handle local commands
        if (input === "disconnect" || input === "exit" || input === "quit") {
            this.#consoleLog("Disconnecting...");
            this.stop();
            return;
        }

        if (input === "ping") {
            this.#sendCommand("ping", []);
            return;
        }

        // Send command to remote server
        const parts = input.split(" ");
        const commandName = parts[0];
        if (!commandName) {
            this.#readline.prompt();
            return;
        }

        const args = parts.slice(1);
        this.#sendCommand(commandName, args);
    }

    #sendCommand(command: string, args: string[]): void {
        if (!this.#ws || !this.#connected) {
            this.#consoleLog("Not connected to remote console");
            this.#readline.prompt();
            return;
        }

        const message = {
            type: "console_command",
            command,
            args,
            timestamp: Date.now()
        };

        try {
            this.#ws.send(JSON.stringify(message));
        } catch (err) {
            this.#consoleLog(`Failed to send command: ${err}`);
            this.#readline.prompt();
        }
    }

    #handleServerMessage(message: ConsoleMessage): void {
        switch (message.type) {
            case "console_response":
                if (message.output) {
                    // Split multiline output and display each line
                    const lines = message.output.split("\\n");
                    for (const line of lines) {
                        if (line.trim()) {
                            this.#consoleLog(line);
                        }
                    }
                }
                this.#readline.prompt();
                break;

            case "console_error":
                this.#consoleLog(`Error: ${message.error}`);
                this.#readline.prompt();
                break;

            case "pong":
                this.#consoleLog(
                    `Pong from ${this.#host}:${this.#port} (round-trip: ${Date.now() - (message.timestamp || 0)}ms)`
                );
                this.#readline.prompt();
                break;

            default:
                debug("Unknown message type:", message);
                this.#readline.prompt();
        }
    }
}

// Helper function to parse host:port string
export function parseRemoteConsoleHost(hostString: string): { host: string; port: number } {
    const defaultPort = 3000;

    if (hostString.includes(":")) {
        const [host, portStr] = hostString.split(":");
        const port = parseInt(portStr as string, 10);

        if (isNaN(port) || port < 1 || port > 65535) {
            throw new Error(`Invalid port number: ${portStr}`);
        }

        return { host: host || "localhost", port };
    }

    return { host: hostString || "localhost", port: defaultPort };
}
