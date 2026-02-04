export interface InputEventData {
    x: number;
    y: number;
    dx?: number | undefined;
    dy?: number | undefined;
    timestamp: number;
    keyboardModifiers: number;
    mouseButtons: number;
    // Additional fields for specific event types
    keycode?: number | undefined;
    text?: string | undefined;
    button?: number | undefined;
}
