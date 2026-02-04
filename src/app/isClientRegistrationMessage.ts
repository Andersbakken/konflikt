import type { ClientRegistrationMessage } from "./ClientRegistrationMessage";

export function isClientRegistrationMessage(obj: unknown): obj is ClientRegistrationMessage {
    if (typeof obj !== "object" || obj === null || !("type" in obj) || obj.type !== "client_registration") {
        return false;
    }

    const msg = obj as Record<string, unknown>;
    return (
        typeof msg.instanceId === "string" &&
        typeof msg.displayName === "string" &&
        typeof msg.machineId === "string" &&
        typeof msg.screenWidth === "number" &&
        typeof msg.screenHeight === "number"
    );
}
