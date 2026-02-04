import type { PreferredPosition } from "./PreferredPosition";
import type { Rect } from "./Rect";

export interface ConnectedInstanceInfo {
    displayId: string;
    machineId: string;
    lastSeen: number;
    screenGeometry: Rect;
    preferredPosition: PreferredPosition;
}
