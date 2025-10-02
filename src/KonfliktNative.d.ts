declare const enum KonfliktMouseButton {
    None = 0x0,
    Left = 0x1,
    Right = 0x2,
    Middle = 0x4
}

declare const enum KonfliktKeyboardModifier {
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

declare interface KonfliktState {
    keyboardModifiers: KonfliktKeyboardModifier;
    mouseButtons: KonfliktMouseButton;
    x: number;
    y: number;
}

declare interface KonfliktEvent extends KonfliktState {
    timestamp: number;
}

declare interface KonfliktMouseMoveEvent extends KonfliktEvent {
    type: "mouseMove";
}

declare interface KonfliktMouseButtonEvent extends KonfliktEvent {
    button: KonfliktMouseButton;
}

declare interface KonfliktMouseButtonPressEvent extends KonfliktMouseButtonEvent {
    type: "mousePress";
}

declare interface KonfliktMouseButtonReleaseEvent extends KonfliktMouseButtonEvent {
    type: "mouseRelease";
}

declare interface KonfliktKeyEvent extends KonfliktEvent {
    keycode: number;
    text: string | undefined;
}

declare interface KonfliktKeyPressEvent extends KonfliktKeyEvent {
    type: "keyPress";
}

declare interface KonfliktKeyReleaseEvent extends KonfliktKeyEvent {
    type: "keyRelease";
}

declare interface KonfliktDesktop {
    width: number;
    height: number;
}

declare interface KonfliktDesktopEvent {
    type: "desktopChanged";
    desktop: KonfliktDesktop;
}

declare class KonfliktNative {
    get desktop(): KonfliktDesktop;
    get state(): KonfliktState;

    on(type: "mousePress", listener: (event: KonfliktMouseButtonPressEvent) => void): void;
    off(type: "mousePress", listener: (event: KonfliktMouseButtonPressEvent) => void): void;

    on(type: "mouseRelease", listener: (event: KonfliktMouseButtonReleaseEvent) => void): void;
    off(type: "mouseRelease", listener: (event: KonfliktMouseButtonReleaseEvent) => void): void;

    on(type: "mouseMove", listener: (event: KonfliktMouseMoveEvent) => void): void;
    off(type: "mouseMove", listener: (event: KonfliktMouseMoveEvent) => void): void;

    on(type: "keyPress", listener: (event: KonfliktKeyPressEvent) => void): void;
    off(type: "keyPress", listener: (event: KonfliktKeyPressEvent) => void): void;

    on(type: "keyRelease", listener: (event: KonfliktKeyReleaseEvent) => void): void;
    off(type: "keyRelease", listener: (event: KonfliktKeyReleaseEvent) => void): void;

    on(type: "desktopChanged", listener: (event: KonfliktDesktopEvent) => void): void;
    off(type: "desktopChanged", listener: (event: KonfliktDesktopEvent) => void): void;

    sendMouseEvent(
        event: KonfliktMouseButtonPressEvent | KonfliktMouseButtonReleaseEvent | KonfliktMouseMoveEvent
    ): void;
    sendKeyEvent(event: KonfliktKeyPressEvent | KonfliktKeyReleaseEvent): void;
}
