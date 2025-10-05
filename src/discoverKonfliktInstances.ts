import { ServiceDiscovery } from "./ServiceDiscovery";
import { verbose } from "./verbose";
import type { DiscoveredService } from "./DiscoveredService";

export async function discoverKonfliktInstances(timeoutMs: number = 3000): Promise<DiscoveredService[]> {
    return new Promise<DiscoveredService[]>((resolve: (services: DiscoveredService[]) => void) => {
        const serviceDiscovery = new ServiceDiscovery();
        const discoveredServices: DiscoveredService[] = [];
        
        verbose(`Starting service discovery for Konflikt instances (timeout: ${timeoutMs}ms)...`);
        
        serviceDiscovery.on("serviceUp", (service: DiscoveredService) => {
            verbose(`Discovered service: ${service.name} at ${service.host}:${service.port} (role: ${service.txt?.role})`);
            discoveredServices.push(service);
        });
        
        serviceDiscovery.startDiscovery();
        
        // Set timeout to resolve with discovered services
        setTimeout(async () => {
            verbose(`Discovery timeout reached, found ${discoveredServices.length} instances`);
            await serviceDiscovery.stop();
            resolve(discoveredServices);
        }, timeoutMs);
    });
}