import type { ScreenInfo } from "../types";

interface ScreenRectProps {
    screen: ScreenInfo;
    scale: number;
    selected: boolean;
    onSelect: () => void;
    onDrag?: (deltaX: number, deltaY: number) => void;
    draggable: boolean;
}

export function ScreenRect({ screen, scale, selected, onSelect, onDrag, draggable }: ScreenRectProps): JSX.Element {
    const handleMouseDown = (e: React.MouseEvent): void => {
        if (!draggable || !onDrag) return;

        e.preventDefault();
        const startX = e.clientX;
        const startY = e.clientY;

        const handleMouseMove = (moveEvent: MouseEvent): void => {
            const deltaX = (moveEvent.clientX - startX) / scale;
            const deltaY = (moveEvent.clientY - startY) / scale;
            onDrag(deltaX, deltaY);
        };

        const handleMouseUp = (): void => {
            document.removeEventListener("mousemove", handleMouseMove);
            document.removeEventListener("mouseup", handleMouseUp);
        };

        document.addEventListener("mousemove", handleMouseMove);
        document.addEventListener("mouseup", handleMouseUp);
    };

    return (
        <div
            onClick={onSelect}
            onMouseDown={handleMouseDown}
            style={{
                position: "absolute",
                left: screen.x * scale,
                top: screen.y * scale,
                width: screen.width * scale,
                height: screen.height * scale,
                background: screen.isServer
                    ? "rgba(0, 212, 255, 0.2)"
                    : screen.online
                      ? "rgba(0, 255, 136, 0.2)"
                      : "rgba(136, 136, 136, 0.2)",
                border: selected ? "2px solid #fff" : "1px solid rgba(255,255,255,0.3)",
                borderRadius: "4px",
                cursor: draggable ? "move" : "pointer",
                display: "flex",
                flexDirection: "column",
                alignItems: "center",
                justifyContent: "center",
                fontSize: `${Math.max(10, 12 * scale)}px`,
                color: screen.online ? "#fff" : "#888",
                userSelect: "none",
                transition: "border 0.1s"
            }}
        >
            <div style={{ fontWeight: 600 }}>{screen.displayName}</div>
            <div style={{ fontSize: "0.8em", opacity: 0.7 }}>
                {screen.width} x {screen.height}
            </div>
            {!screen.online && <div style={{ fontSize: "0.7em", color: "#f55", marginTop: "4px" }}>OFFLINE</div>}
            {screen.isServer && (
                <div
                    style={{
                        fontSize: "0.7em",
                        color: "#00d4ff",
                        marginTop: "4px"
                    }}
                >
                    SERVER
                </div>
            )}
        </div>
    );
}
