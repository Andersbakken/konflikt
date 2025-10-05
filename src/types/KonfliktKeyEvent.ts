import type { KonfliktEvent } from "./KonfliktEvent";

export interface KonfliktKeyEvent extends KonfliktEvent {
    keycode: number;
    text: string | undefined;
}