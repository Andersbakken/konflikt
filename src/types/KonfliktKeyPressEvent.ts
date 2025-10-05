import type { KonfliktKeyEvent } from "./KonfliktKeyEvent";

export interface KonfliktKeyPressEvent extends KonfliktKeyEvent {
    type: "keyPress";
}