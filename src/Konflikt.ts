import { Console } from "./Console.js";
import { KonfliktNative as KonfliktNativeConstructor } from "./native.js";
import { Server } from "./Server.js";
import { createNativeLogger, verbose } from "./Log";
import type { Config } from "./Config.js";
import type {
    KonfliktKeyPressEvent,
    KonfliktKeyReleaseEvent,
    KonfliktMouseButtonPressEvent,
    KonfliktMouseButtonReleaseEvent,
    KonfliktMouseMoveEvent
} from "./KonfliktNative";
import type { KonfliktNative } from "./KonfliktNative.js";

export class Konflikt {
    #config: Config;
    #native: KonfliktNative;
    #server: Server;
    #console: Console | undefined;

    constructor(config: Config) {
        this.#config = config;
        this.#native = new KonfliktNativeConstructor(createNativeLogger());
        this.#server = new Server(
            config.port,
            config.instanceId,
            config.instanceName
        );

        // Console will be created during init if stdin is available
        this.#native.on("keyPress", this.#onKeyPress.bind(this));
        this.#native.on("keyRelease", this.#onKeyRelease.bind(this));
        this.#native.on("mousePress", this.#onMousePress.bind(this));
        this.#native.on("mouseRelease", this.#onMouseRelease.bind(this));
        this.#native.on("mouseMove", this.#onMouseMove.bind(this));
    }

    get config(): Config {
        return this.#config;
    }

    get native(): KonfliktNative {
        return this.#native;
    }

    get server(): Server {
        return this.#server;
    }

    get console(): Console | undefined {
        return this.#console;
    }

    async init(): Promise<void> {
        verbose("Initializing Konflikt...", this.#config);
        
        // Pass config to server for console commands
        this.#server.setConfig(this.#config);
        
        await this.#server.start();
        
        // Start console based on configuration
        const consoleConfig = this.#config.console;
        if (consoleConfig === true) {
            try {
                if (process.stdin.isTTY && process.stdout.isTTY) {
                    this.#console = new Console(this);
                    this.#console.start();
                } else {
                    verbose("Non-interactive environment detected, console disabled");
                }
            } catch (err) {
                verbose("Console initialization failed:", err);
                // Continue without console
            }
        } else if (consoleConfig === false) {
            verbose("Console disabled by configuration");
        }
        // Remote console mode is handled in index.ts and doesn't reach here
    }

    // eslint-disable-next-line class-methods-use-this
    #onKeyPress(event: KonfliktKeyPressEvent): void {
        verbose("Key pressed:", event);
    }

    // eslint-disable-next-line class-methods-use-this
    #onKeyRelease(event: KonfliktKeyReleaseEvent): void {
        verbose("Key released:", event);
    }

    // eslint-disable-next-line class-methods-use-this
    #onMousePress(event: KonfliktMouseButtonPressEvent): void {
        verbose("Mouse button pressed:", event);
    }

    // eslint-disable-next-line class-methods-use-this
    #onMouseRelease(event: KonfliktMouseButtonReleaseEvent): void {
        verbose("Mouse button released:", event);
    }

    // eslint-disable-next-line class-methods-use-this
    #onMouseMove(event: KonfliktMouseMoveEvent): void {
        verbose("Mouse moved:", event);
    }
}
