import { Config } from "./Config.js";
import { KonfliktNative as KonfliktNativeConstructor } from "./native.js";
import type { KonfliktNative } from "./KonfliktNative.js";

export class Konflikt {
    #config: Config;
    #native: KonfliktNative;

    constructor(configPath?: string) {
        this.#config = new Config(configPath);
        this.#native = new KonfliktNativeConstructor();
        this.#native.on("keyPress", console.log.bind(console, "Key pressed event from native:"));
        this.#native.on("keyRelease", console.log.bind(console, "Key released event from native:"));
        this.#native.on("mousePress", console.log.bind(console, "Mouse press event from native:"));
        this.#native.on("mouseRelease", console.log.bind(console, "Mouse release event from native:"));
        this.#native.on("mouseMove", console.log.bind(console, "Mouse move event from native:"));
    }

    get config(): Config {
        return this.#config;
    }

    get native(): KonfliktNative {
        return this.#native;
    }

    init(): void {
        console.log("Initializing Konflikt...", this.#config);
    }
}
