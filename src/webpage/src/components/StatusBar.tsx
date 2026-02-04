import { Link } from "react-router-dom";
import type { StatusResponse } from "../types";

interface StatusBarProps {
    status: StatusResponse;
}

export function StatusBar({ status }: StatusBarProps): JSX.Element {
    const isServer = status.role === "server";

    return (
        <header
            style={{
                display: "flex",
                justifyContent: "space-between",
                alignItems: "center",
                padding: "12px 20px",
                background: "#16213e",
                borderBottom: "1px solid #0f3460"
            }}
        >
            <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>
                <h1 style={{ fontSize: "18px", fontWeight: 600 }}>Konflikt</h1>
                <span
                    style={{
                        padding: "4px 8px",
                        borderRadius: "4px",
                        fontSize: "12px",
                        fontWeight: 500,
                        background: isServer ? "#0f3460" : "#1a1a2e",
                        color: isServer ? "#00d4ff" : "#888"
                    }}
                >
                    {status.role.toUpperCase()}
                </span>
                <span style={{ color: "#888", fontSize: "14px" }}>{status.instanceName}</span>
            </div>
            <nav style={{ display: "flex", gap: "16px" }}>
                <Link
                    to="/"
                    style={{
                        color: "#eee",
                        textDecoration: "none",
                        fontSize: "14px"
                    }}
                >
                    Layout
                </Link>
                <Link
                    to="/settings"
                    style={{
                        color: "#eee",
                        textDecoration: "none",
                        fontSize: "14px"
                    }}
                >
                    Settings
                </Link>
            </nav>
        </header>
    );
}
