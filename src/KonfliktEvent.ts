import type { KonfliktState } from "./KonfliktState";

export interface KonfliktEvent extends KonfliktState {
    timestamp: number;
}