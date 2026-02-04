import { useEffect, useState, useCallback } from "react";
import { ScreenRect } from "./ScreenRect";
import { fetchLayout } from "../api/client";
import type { ScreenInfo } from "../types";

export function LayoutView(): JSX.Element {
    const [screens, setScreens] = useState<ScreenInfo[]>([]);
    const [selectedId, setSelectedId] = useState<string | null>(null);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);

    const loadLayout = useCallback(async () => {
        try {
            const data = await fetchLayout();
            setScreens(data.screens);
            setError(null);
        } catch (err) {
            setError(err instanceof Error ? err.message : "Failed to load layout");
        } finally {
            setLoading(false);
        }
    }, []);

    useEffect(() => {
        void loadLayout();
        // Poll for updates every 5 seconds
        const interval = setInterval(() => void loadLayout(), 5000);
        return () => clearInterval(interval);
    }, [loadLayout]);

    // Calculate scale to fit all screens in viewport
    const calculateScale = (): number => {
        if (screens.length === 0) return 0.2;
        const maxRight = Math.max(...screens.map((s) => s.x + s.width));
        const maxBottom = Math.max(...screens.map((s) => s.y + s.height));
        const viewWidth = 800;
        const viewHeight = 500;
        const scaleX = viewWidth / maxRight;
        const scaleY = viewHeight / maxBottom;
        return Math.min(scaleX, scaleY, 0.3);
    };

    const scale = calculateScale();

    if (loading) {
        return <div>Loading layout...</div>;
    }

    if (error) {
        return (
            <div>
                <p style={{ color: "#f55" }}>Error: {error}</p>
                <button onClick={() => void loadLayout()}>Retry</button>
            </div>
        );
    }

    return (
        <div>
            <div style={{ marginBottom: "20px" }}>
                <h2>Screen Layout</h2>
                <p style={{ color: "#888", fontSize: "14px" }}>
                    Layout is managed by the server. Connect to server UI to edit.
                </p>
            </div>

            <div
                style={{
                    position: "relative",
                    width: "100%",
                    height: "500px",
                    background: "#0a0a15",
                    borderRadius: "8px",
                    overflow: "hidden"
                }}
            >
                {screens.map((screen) => (
                    <ScreenRect
                        key={screen.instanceId}
                        screen={screen}
                        scale={scale}
                        selected={selectedId === screen.instanceId}
                        onSelect={() => setSelectedId(screen.instanceId)}
                        draggable={false}
                    />
                ))}
            </div>

            {selectedId && (
                <div
                    style={{
                        marginTop: "20px",
                        padding: "16px",
                        background: "#16213e",
                        borderRadius: "8px"
                    }}
                >
                    <h3>Screen Details</h3>
                    {(() => {
                        const selected = screens.find((s) => s.instanceId === selectedId);
                        if (!selected) return null;
                        return (
                            <div
                                style={{
                                    display: "grid",
                                    gridTemplateColumns: "120px 1fr",
                                    gap: "8px",
                                    marginTop: "12px"
                                }}
                            >
                                <span>Name:</span>
                                <span>{selected.displayName}</span>
                                <span>Position:</span>
                                <span>
                                    ({selected.x}, {selected.y})
                                </span>
                                <span>Size:</span>
                                <span>
                                    {selected.width} x {selected.height}
                                </span>
                                <span>Status:</span>
                                <span style={{ color: selected.online ? "#0f8" : "#f55" }}>
                                    {selected.online ? "Online" : "Offline"}
                                </span>
                            </div>
                        );
                    })()}
                </div>
            )}
        </div>
    );
}
