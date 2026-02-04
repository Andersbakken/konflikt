import { BrowserRouter, Routes, Route, Navigate } from "react-router-dom";
import { useEffect, useState } from "react";
import { LayoutEditor } from "./components/LayoutEditor";
import { LayoutView } from "./components/LayoutView";
import { SettingsForm } from "./components/SettingsForm";
import { StatusBar } from "./components/StatusBar";
import { fetchStatus } from "./api/client";
import type { StatusResponse } from "./types/StatusResponse";

export function App(): JSX.Element {
    const [status, setStatus] = useState<StatusResponse | null>(null);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        fetchStatus()
            .then(setStatus)
            .catch((err: Error) => setError(err.message));
    }, []);

    if (error) {
        return (
            <div style={{ padding: "20px", textAlign: "center" }}>
                <h1>Connection Error</h1>
                <p>{error}</p>
                <p>Make sure Konflikt is running on this machine.</p>
            </div>
        );
    }

    if (!status) {
        return (
            <div style={{ padding: "20px", textAlign: "center" }}>
                <p>Connecting to Konflikt...</p>
            </div>
        );
    }

    const isServer = status.role === "server";

    return (
        <BrowserRouter basename="/ui">
            <div style={{ display: "flex", flexDirection: "column", minHeight: "100vh" }}>
                <StatusBar status={status} />
                <main style={{ flex: 1, padding: "20px" }}>
                    <Routes>
                        <Route path="/" element={isServer ? <LayoutEditor /> : <LayoutView />} />
                        <Route path="/settings" element={<SettingsForm />} />
                        <Route path="*" element={<Navigate to="/" replace />} />
                    </Routes>
                </main>
            </div>
        </BrowserRouter>
    );
}
