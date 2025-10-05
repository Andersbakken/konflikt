import type { KonfliktDesktop } from "./KonfliktDesktop";

export interface KonfliktDesktopEvent {
    type: "desktop";
    desktop: KonfliktDesktop;
}