import type { KonfliktKeyboardModifier } from "./KonfliktKeyboardModifier";
import type { KonfliktMouseButton } from "./KonfliktMouseButton";

export interface KonfliktState {
    keyboardModifiers: KonfliktKeyboardModifier;
    mouseButtons: KonfliktMouseButton;
    x: number;
    y: number;
}