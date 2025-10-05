import { debug } from "./debug";
import { error } from "./error";
import { log } from "./log";
import { verbose } from "./verbose";
import type { NativeLoggerCallbacks } from "./KonfliktNative";

export function createNativeLogger(): NativeLoggerCallbacks {
    return {
        verbose: (message: string): void => {
            verbose(`[Native] ${message}`);
        },
        debug: (message: string): void => {
            debug(`[Native] ${message}`);
        },
        log: (message: string): void => {
            log(`[Native] ${message}`);
        },
        error: (message: string): void => {
            error(`[Native] ${message}`);
        }
    };
}