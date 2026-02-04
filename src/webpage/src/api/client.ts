import type { ConfigResponse } from "../types/ConfigResponse";
import type { LayoutResponse } from "../types/LayoutResponse";
import type { ScreenPositionUpdate } from "../types/ScreenPositionUpdate";
import type { StatusResponse } from "../types/StatusResponse";
import type { SuccessResponse } from "../types/SuccessResponse";
import type { UpdateConfigResponse } from "../types/UpdateConfigResponse";
import type { UpdateLayoutResponse } from "../types/UpdateLayoutResponse";
import type { UpdateScreenResponse } from "../types/UpdateScreenResponse";

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

export async function updateLayout(screens: ScreenPositionUpdate[]): Promise<UpdateLayoutResponse> {
    return fetchJson(`${API_BASE}/layout`, {
        method: "PUT",
        body: JSON.stringify({ screens })
    });
}

export async function updateScreenPosition(instanceId: string, x: number, y: number): Promise<UpdateScreenResponse> {
    return fetchJson(`${API_BASE}/layout/${instanceId}`, {
        method: "PATCH",
        body: JSON.stringify({ x, y })
    });
}

export async function removeScreen(instanceId: string): Promise<SuccessResponse> {
    return fetchJson(`${API_BASE}/layout/${instanceId}`, {
        method: "DELETE"
    });
}

export async function fetchConfig(): Promise<ConfigResponse> {
    return fetchJson<ConfigResponse>(`${API_BASE}/config`);
}

export async function updateConfig(config: Partial<ConfigResponse>): Promise<UpdateConfigResponse> {
    return fetchJson(`${API_BASE}/config`, {
        method: "PUT",
        body: JSON.stringify(config)
    });
}
