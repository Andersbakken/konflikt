import { useEffect, useState } from "react";
import { fetchConfig, updateConfig } from "../api/client";
import type { ConfigResponse } from "../types";

export function SettingsForm(): JSX.Element {
    const [config, setConfig] = useState<ConfigResponse | null>(null);
    const [displayName, setDisplayName] = useState("");
    const [loading, setLoading] = useState(true);
    const [saving, setSaving] = useState(false);
    const [error, setError] = useState<string | null>(null);
    const [message, setMessage] = useState<string | null>(null);

    useEffect(() => {
        fetchConfig()
            .then((data) => {
                setConfig(data);
                setDisplayName(data.displayName);
            })
            .catch((err: Error) => setError(err.message))
            .finally(() => setLoading(false));
    }, []);

    const handleSubmit = async (e: React.FormEvent): Promise<void> => {
        e.preventDefault();
        setSaving(true);
        setMessage(null);
        setError(null);

        try {
            const result = await updateConfig({ displayName });
            setMessage(result.message || "Settings saved");
        } catch (err) {
            setError(err instanceof Error ? err.message : "Failed to save settings");
        } finally {
            setSaving(false);
        }
    };

    if (loading) {
        return <div>Loading settings...</div>;
    }

    if (!config) {
        return <div style={{ color: "#f55" }}>Failed to load settings</div>;
    }

    return (
        <div style={{ maxWidth: "500px" }}>
            <h2>Settings</h2>

            <form
                onSubmit={(e) => void handleSubmit(e)}
                style={{ marginTop: "20px" }}
            >
                <div style={{ marginBottom: "16px" }}>
                    <label
                        htmlFor="displayName"
                        style={{ display: "block", marginBottom: "8px" }}
                    >
                        Display Name
                    </label>
                    <input
                        id="displayName"
                        type="text"
                        value={displayName}
                        onChange={(e) => setDisplayName(e.target.value)}
                        style={{
                            width: "100%",
                            padding: "8px 12px",
                            background: "#0a0a15",
                            border: "1px solid #333",
                            borderRadius: "4px",
                            color: "#eee",
                            fontSize: "14px"
                        }}
                    />
                </div>

                <div
                    style={{
                        padding: "16px",
                        background: "#16213e",
                        borderRadius: "8px",
                        marginBottom: "16px"
                    }}
                >
                    <h4 style={{ marginBottom: "12px" }}>Current Configuration</h4>
                    <div
                        style={{
                            display: "grid",
                            gridTemplateColumns: "120px 1fr",
                            gap: "8px",
                            fontSize: "14px"
                        }}
                    >
                        <span style={{ color: "#888" }}>Role:</span>
                        <span>{config.role}</span>
                        <span style={{ color: "#888" }}>Log Level:</span>
                        <span>{config.logLevel}</span>
                        <span style={{ color: "#888" }}>Discovery:</span>
                        <span>{config.discoveryEnabled ? "Enabled" : "Disabled"}</span>
                    </div>
                </div>

                {error && (
                    <div
                        style={{
                            padding: "12px",
                            background: "rgba(255, 85, 85, 0.2)",
                            border: "1px solid #f55",
                            borderRadius: "4px",
                            marginBottom: "16px",
                            color: "#f55"
                        }}
                    >
                        {error}
                    </div>
                )}

                {message && (
                    <div
                        style={{
                            padding: "12px",
                            background: "rgba(0, 255, 136, 0.2)",
                            border: "1px solid #0f8",
                            borderRadius: "4px",
                            marginBottom: "16px",
                            color: "#0f8"
                        }}
                    >
                        {message}
                    </div>
                )}

                <button
                    type="submit"
                    disabled={saving}
                    style={{
                        padding: "10px 20px",
                        background: "#00d4ff",
                        border: "none",
                        borderRadius: "4px",
                        color: "#000",
                        fontSize: "14px",
                        fontWeight: 500,
                        cursor: saving ? "not-allowed" : "pointer",
                        opacity: saving ? 0.7 : 1
                    }}
                >
                    {saving ? "Saving..." : "Save Settings"}
                </button>
            </form>
        </div>
    );
}
