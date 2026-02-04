import type { ScreenInfo } from "./ScreenInfo";

export interface ScreenRectProps {
    screen: ScreenInfo;
    scale: number;
    selected: boolean;
    onSelect: () => void;
    onDrag?: (deltaX: number, deltaY: number) => void;
    draggable: boolean;
}
