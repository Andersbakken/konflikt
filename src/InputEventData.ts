export interface InputEventData {
    x: number;
    y: number;
    timestamp: number;
    keyboardModifiers: number;
    mouseButtons: number;
    // Additional fields for specific event types
    keycode?: number | undefined;
    text?: string | undefined;
    button?: number | undefined;
}
