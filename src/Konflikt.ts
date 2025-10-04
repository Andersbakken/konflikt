import { Config } from "./Config.js";
import { KonfliktNative as KonfliktNativeConstructor } from "./native.js";
import { Server } from "./Server.js";
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

    constructor(configPath?: string) {
        this.#config = new Config(configPath);
        this.#native = new KonfliktNativeConstructor();
        this.#server = new Server();

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

    async init(): Promise<void> {
        console.log("Initializing Konflikt...", this.#config);
        await this.#server.start();
    }

    // eslint-disable-next-line class-methods-use-this
    #onKeyPress(event: KonfliktKeyPressEvent): void {
        console.log("Key pressed:", event);
    }

    // eslint-disable-next-line class-methods-use-this
    #onKeyRelease(event: KonfliktKeyReleaseEvent): void {
        console.log("Key released:", event);
    }

    // eslint-disable-next-line class-methods-use-this
    #onMousePress(event: KonfliktMouseButtonPressEvent): void {
        console.log("Mouse button pressed:", event);
    }

    // eslint-disable-next-line class-methods-use-this
    #onMouseRelease(event: KonfliktMouseButtonReleaseEvent): void {
        console.log("Mouse button released:", event);
    }

    // eslint-disable-next-line class-methods-use-this
    #onMouseMove(event: KonfliktMouseMoveEvent): void {
        console.log("Mouse moved:", event);
    }
}
