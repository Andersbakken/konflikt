import type { ConfigResponse, LayoutResponse, ScreenInfo, StatusResponse } from "../types";

const API_BASE = "/api";

async function fetchJson<T>(url: string, options?: RequestInit): Promise<T> {
    const response = await fetch(url, {
        ...options,
        headers: {
            "Content-Type": "application/json",
            ...options?.headers
        }
    });

    if (!response.ok) {
        const error = await response.text();
        throw new Error(`API error: ${response.status} - ${error}`);
    }

    return response.json() as Promise<T>;
}

export async function fetchStatus(): Promise<StatusResponse> {
    return fetchJson<StatusResponse>(`${API_BASE}/status`);
}

export async function fetchLayout(): Promise<LayoutResponse> {
    return fetchJson<LayoutResponse>(`${API_BASE}/layout`);
}

export async function updateLayout(
    screens: Array<{ instanceId: string; x: number; y: number }>
): Promise<{ success: boolean; screens: ScreenInfo[] }> {
    return fetchJson(`${API_BASE}/layout`, {
        method: "PUT",
        body: JSON.stringify({ screens })
    });
}

export async function updateScreenPosition(
    instanceId: string,
    x: number,
    y: number
): Promise<{ success: boolean; screen: ScreenInfo }> {
    return fetchJson(`${API_BASE}/layout/${instanceId}`, {
        method: "PATCH",
        body: JSON.stringify({ x, y })
    });
}

export async function removeScreen(instanceId: string): Promise<{ success: boolean }> {
    return fetchJson(`${API_BASE}/layout/${instanceId}`, {
        method: "DELETE"
    });
}

export async function fetchConfig(): Promise<ConfigResponse> {
    return fetchJson<ConfigResponse>(`${API_BASE}/config`);
}

export async function updateConfig(
    config: Partial<ConfigResponse>
): Promise<{ success: boolean; message?: string }> {
    return fetchJson(`${API_BASE}/config`, {
        method: "PUT",
        body: JSON.stringify(config)
    });
}
