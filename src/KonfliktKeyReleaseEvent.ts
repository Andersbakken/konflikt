import type { KonfliktKeyEvent } from "./KonfliktKeyEvent";

export interface KonfliktKeyReleaseEvent extends KonfliktKeyEvent {
    type: "keyRelease";
}