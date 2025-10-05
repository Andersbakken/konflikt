import type { ConfigType } from "./ConfigSchema";

export interface ConvictInstance {
    get(key: string): unknown;
    set(key: string, value: unknown): void;
    getProperties(): ConfigType;
    load(obj: Record<string, unknown>, options?: { args?: string[] }): void;
    validate(options?: { allowed?: string }): void;
}
