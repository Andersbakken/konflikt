import type { BaseMessage } from "./BaseMessage";

export function createBaseMessage(source: string): BaseMessage {
    return {
        id: crypto.randomUUID(),
        timestamp: Date.now(),
        source
    };
}
