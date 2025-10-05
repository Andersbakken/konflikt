export type Side = "left" | "right" | "top" | "bottom";

export type Alignment = "start" | "center" | "end";

export interface ScreenGeometry {
    x: number;
    y: number;
    width: number;
    height: number;
}

export interface PreferredPosition {
    side: Side;
    alignment?: Alignment;
}

export interface ClientScreenInfo {
    geometry: ScreenGeometry;
    position: PreferredPosition;
}