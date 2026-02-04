import type { ScreenInfo } from "./ScreenInfo";

export interface LayoutUpdateMessage {
    type: "layout_update";
    screens: ScreenInfo[];
    timestamp: number;
}
