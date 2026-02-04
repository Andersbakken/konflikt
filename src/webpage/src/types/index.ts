export interface ScreenInfo {
    instanceId: string;
    displayName: string;
    machineId: string;
    x: number;
    y: number;
    width: number;
    height: number;
    isServer: boolean;
    online: boolean;
}

export interface Adjacency {
    left?: string;
    right?: string;
    top?: string;
    bottom?: string;
}

export interface StatusResponse {
    role: string;
    instanceId: string;
    instanceName: string;
    startTime: number;
    uptime: number;
    port: number | null;
    version: string;
}

export interface LayoutResponse {
    screens: ScreenInfo[];
}

export interface ConfigResponse {
    displayName: string;
    role: string;
    logLevel: string;
    discoveryEnabled: boolean;
}
