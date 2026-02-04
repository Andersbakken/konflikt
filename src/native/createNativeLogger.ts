import { debug } from "../app/debug";
import { error } from "../app/error";
import { log } from "../app/log";
import { verbose } from "../app/verbose";
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
