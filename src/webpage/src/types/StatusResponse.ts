export interface StatusResponse {
    role: string;
    instanceId: string;
    instanceName: string;
    startTime: number;
    uptime: number;
    port: number | null;
    version: string;
}
