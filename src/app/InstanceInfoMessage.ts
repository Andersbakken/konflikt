import type { Rect } from "./Rect";

export interface InstanceInfoMessage {
    type: "instance_info";
    instanceId: string;
    displayId: string;
    machineId: string;
    timestamp: number;
    screenGeometry: Rect;
}
