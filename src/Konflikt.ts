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

    constructor(config: Config) {
        this.#config = config;
        this.#native = new KonfliktNativeConstructor(createNativeLogger());
        this.#server = new Server(
            config.get("network.port") as number,
            config.get("instance.id") as string,
            config.get("instance.name") as string
        );

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
        verbose("Initializing Konflikt...", this.#config);
        await this.#server.start();
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
