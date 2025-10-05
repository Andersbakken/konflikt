import type { KonfliktEvent } from "./KonfliktEvent";
import type { KonfliktMouseButton } from "./KonfliktMouseButton";

export interface KonfliktMouseButtonEvent extends KonfliktEvent {
    button: KonfliktMouseButton;
}