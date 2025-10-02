import { Config } from "./Config.js";
import { KonfliktNative } from "./native.js";

export class Konflikt {
    #config: Config;
    #native: KonfliktNative;

    constructor(configPath?: string) {
        this.#config = new Config(configPath);
        this.#native = new KonfliktNative();
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
