import type { ScreenInfo } from "./ScreenInfo";

export interface UpdateLayoutResponse {
    success: boolean;
    screens: ScreenInfo[];
}
