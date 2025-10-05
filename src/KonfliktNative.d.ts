export const enum KonfliktMouseButton {
    None = 0x0,
    Left = 0x1,
    Right = 0x2,
    Middle = 0x4
}

export const enum KonfliktKeyboardModifier {
    None = 0x000,
    LeftShift = 0x001,
    RightShift = 0x002,
    LeftAlt = 0x004,
    RightAlt = 0x008,
    LeftControl = 0x010,
    RightControl = 0x020,
    LeftSuper = 0x040,
    RightSuper = 0x080,
    CapsLock = 0x100,
    NumLock = 0x200,
    ScrollLock = 0x400
}

export interface KonfliktState {
    keyboardModifiers: KonfliktKeyboardModifier;
    mouseButtons: KonfliktMouseButton;
    x: number;
    y: number;
    dx: number; // Relative X delta since last event
    dy: number; // Relative Y delta since last event
}

export interface KonfliktEvent extends KonfliktState {
    timestamp: number;
}

export interface KonfliktMouseMoveEvent extends KonfliktEvent {
    type: "mouseMove";
}

export interface KonfliktMouseButtonEvent extends KonfliktEvent {
    button: KonfliktMouseButton;
}

export interface KonfliktMouseButtonPressEvent extends KonfliktMouseButtonEvent {
    type: "mousePress";
}

export interface KonfliktMouseButtonReleaseEvent extends KonfliktMouseButtonEvent {
    type: "mouseRelease";
}

export interface KonfliktKeyEvent extends KonfliktEvent {
    keycode: number;
    text: string | undefined;
}

export interface KonfliktKeyPressEvent extends KonfliktKeyEvent {
    type: "keyPress";
}

export interface KonfliktKeyReleaseEvent extends KonfliktKeyEvent {
    type: "keyRelease";
}

export interface KonfliktDesktop {
    width: number;
    height: number;
}

export interface KonfliktDesktopEvent {
    type: "desktopChanged";
    desktop: KonfliktDesktop;
}

export interface NativeLoggerCallbacks {
    verbose(message: string): void;
    debug(message: string): void;
    log(message: string): void;
    error(message: string): void;
}

export declare class KonfliktNative {
    constructor(logger: NativeLoggerCallbacks);

    get desktop(): KonfliktDesktop;
    get state(): KonfliktState;

    on(type: "desktopChanged", listener: (event: KonfliktDesktopEvent) => void): void;
    on(type: "keyPress", listener: (event: KonfliktKeyPressEvent) => void): void;
    on(type: "keyRelease", listener: (event: KonfliktKeyReleaseEvent) => void): void;
    on(type: "mouseMove", listener: (event: KonfliktMouseMoveEvent) => void): void;
    on(type: "mousePress", listener: (event: KonfliktMouseButtonPressEvent) => void): void;
    on(type: "mouseRelease", listener: (event: KonfliktMouseButtonReleaseEvent) => void): void;

    off(type: "desktopChanged", listener: (event: KonfliktDesktopEvent) => void): void;
    off(type: "keyPress", listener: (event: KonfliktKeyPressEvent) => void): void;
    off(type: "keyRelease", listener: (event: KonfliktKeyReleaseEvent) => void): void;
    off(type: "mouseMove", listener: (event: KonfliktMouseMoveEvent) => void): void;
    off(type: "mousePress", listener: (event: KonfliktMouseButtonPressEvent) => void): void;
    off(type: "mouseRelease", listener: (event: KonfliktMouseButtonReleaseEvent) => void): void;

    sendKeyEvent(event: KonfliktKeyPressEvent | KonfliktKeyReleaseEvent): void;
    sendMouseEvent(
        event: KonfliktMouseButtonPressEvent | KonfliktMouseButtonReleaseEvent | KonfliktMouseMoveEvent
    ): void;
}
