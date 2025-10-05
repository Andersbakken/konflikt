import type { Alignment } from "./Alignment";
import type { Side } from "./Side";

export interface PreferredPosition {
    side: Side;
    alignment?: Alignment;
}
