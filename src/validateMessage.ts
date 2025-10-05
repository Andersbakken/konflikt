import { MessageSchema } from "./MessageSchema";
import type { Message } from "./Message";

export function validateMessage(data: unknown): Message | null {
    try {
        return MessageSchema.parse(data);
    } catch {
        return null;
    }
}
