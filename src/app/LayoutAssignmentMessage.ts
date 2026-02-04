import type { Adjacency } from "./Adjacency";
import type { ScreenInfo } from "./ScreenInfo";

export interface LayoutAssignmentMessage {
    type: "layout_assignment";
    position: { x: number; y: number };
    adjacency: Adjacency;
    fullLayout: ScreenInfo[];
}
