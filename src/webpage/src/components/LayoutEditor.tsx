import { useEffect, useState, useCallback } from "react";
import { ScreenRect } from "./ScreenRect";
import { fetchLayout, updateLayout } from "../api/client";
import type { ScreenInfo } from "../types";

export function LayoutEditor(): JSX.Element {
    const [screens, setScreens] = useState<ScreenInfo[]>([]);
    const [selectedId, setSelectedId] = useState<string | null>(null);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [dirty, setDirty] = useState(false);
    const [saving, setSaving] = useState(false);

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
    }, [loadLayout]);

    const handleDrag = useCallback((instanceId: string, deltaX: number, deltaY: number) => {
        setScreens((prev) =>
            prev.map((s) =>
                s.instanceId === instanceId ? { ...s, x: Math.round(s.x + deltaX), y: Math.round(s.y + deltaY) } : s
            )
        );
        setDirty(true);
    }, []);

    const handleSave = async (): Promise<void> => {
        setSaving(true);
        try {
            const updates = screens.map((s) => ({
                instanceId: s.instanceId,
                x: s.x,
                y: s.y
            }));
            await updateLayout(updates);
            setDirty(false);
        } catch (err) {
            setError(err instanceof Error ? err.message : "Failed to save layout");
        } finally {
            setSaving(false);
        }
    };

    const handleAutoArrange = (): void => {
        let currentX = 0;
        const arranged = screens.map((s) => {
            const newScreen = { ...s, x: currentX, y: 0 };
            currentX += s.width;
            return newScreen;
        });
        setScreens(arranged);
        setDirty(true);
    };

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
            <div
                style={{
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center",
                    marginBottom: "20px"
                }}
            >
                <h2>Screen Layout</h2>
                <div style={{ display: "flex", gap: "10px" }}>
                    <button
                        onClick={handleAutoArrange}
                        style={{
                            padding: "8px 16px",
                            background: "#0f3460",
                            border: "none",
                            borderRadius: "4px",
                            color: "#fff",
                            cursor: "pointer"
                        }}
                    >
                        Auto Arrange
                    </button>
                    <button
                        onClick={() => void handleSave()}
                        disabled={!dirty || saving}
                        style={{
                            padding: "8px 16px",
                            background: dirty ? "#00d4ff" : "#444",
                            border: "none",
                            borderRadius: "4px",
                            color: dirty ? "#000" : "#888",
                            cursor: dirty ? "pointer" : "not-allowed"
                        }}
                    >
                        {saving ? "Saving..." : "Save Layout"}
                    </button>
                </div>
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
                        onDrag={(dx, dy) => handleDrag(screen.instanceId, dx, dy)}
                        draggable={true}
                    />
                ))}
            </div>

            {selectedId && (
                <div style={{ marginTop: "20px", padding: "16px", background: "#16213e", borderRadius: "8px" }}>
                    <h3>Selected Screen</h3>
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
