import type { InstanceInfoMessage } from "./InstanceInfoMessage";

export function isInstanceInfoMessage(obj: unknown): obj is InstanceInfoMessage {
    if (typeof obj !== "object" || obj === null || !("type" in obj) || obj.type !== "instance_info") {
        return false;
    }

    const msg = obj as Record<string, unknown>;
    return (
        typeof msg.instanceId === "string" &&
        typeof msg.displayId === "string" &&
        typeof msg.machineId === "string" &&
        typeof msg.timestamp === "number"
    );
}
