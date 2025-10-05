import type { KonfliktMouseButtonEvent } from "./KonfliktMouseButtonEvent";

export interface KonfliktMouseButtonReleaseEvent extends KonfliktMouseButtonEvent {
    type: "mouseRelease";
}