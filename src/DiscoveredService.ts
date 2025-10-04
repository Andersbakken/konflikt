export interface DiscoveredService {
    name: string;
    host: string;
    port: number;
    addresses: string[];
    txt?: Record<string, unknown>;
}
