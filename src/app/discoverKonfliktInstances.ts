import { ServiceDiscovery } from "./ServiceDiscovery";
import { verbose } from "./verbose";
import type { DiscoveredService } from "./DiscoveredService";

export async function discoverKonfliktInstances(timeoutMs: number = 3000): Promise<DiscoveredService[]> {
    return new Promise<DiscoveredService[]>((res: (services: DiscoveredService[]) => void) => {
        const serviceDiscovery = new ServiceDiscovery();
        const discoveredServices: DiscoveredService[] = [];

        const resolve = async (): Promise<void> => {
            await serviceDiscovery.stop();
            res(discoveredServices);
        };

        verbose(`Starting service discovery for Konflikt instances (timeout: ${timeoutMs}ms)...`);

        serviceDiscovery.on("serviceUp", (service: DiscoveredService) => {
            verbose(
                `Discovered service: ${service.name} at ${service.host}:${service.port} (role: ${service.txt?.role})`
            );
            discoveredServices.push(service);
            if (discoveredServices.length === 1) {
                setTimeout(() => void resolve(), 500);
            }
        });

        serviceDiscovery.startDiscovery();

        // Set timeout to resolve with discovered services
        setTimeout(() => {
            verbose(`Discovery timeout reached, found ${discoveredServices.length} instances`);
            void resolve();
        }, 3000);
    });
}
