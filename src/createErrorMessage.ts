import { createBaseMessage } from "./createBaseMessage";
import type { ErrorMessage } from "./ErrorMessage";

export function createErrorMessage(source: string, code: string, message: string, details?: unknown): ErrorMessage {
    return {
        ...createBaseMessage(source),
        type: "error",
        code,
        message,
        details
    };
}
