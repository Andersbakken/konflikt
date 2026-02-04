import type { DiscoveredService } from "./DiscoveredService";
import type { Rect } from "./Rect";

export interface PendingReconnect {
    service: DiscoveredService;
    screenGeometry: Rect | undefined;
    attempts: number;
}
