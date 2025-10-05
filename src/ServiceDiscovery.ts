import { Bonjour } from "bonjour-service";
import { EventEmitter } from "events";
import { debug } from "./debug";
import { error } from "./error";
import { log } from "./log";
import type { DiscoveredService } from "./DiscoveredService";
import type { Service } from "bonjour-service";

export class ServiceDiscovery extends EventEmitter {
    #bonjour: Bonjour;
    #advertisedService: Service | undefined;
    #browser: ReturnType<Bonjour["find"]> | undefined;
    #discoveredServices = new Map<string, DiscoveredService>();

    constructor() {
        super();
        this.#bonjour = new Bonjour();
    }

    /**
     * Advertise this instance as a Konflikt service
     */
    advertise(port: number, instanceName?: string): void {
        const serviceName = instanceName || `konflikt-${process.pid}`;

        debug(`Advertising Konflikt service "${serviceName}" on port ${port}`);

        this.#advertisedService = this.#bonjour.publish({
            name: serviceName,
            type: "konflikt",
            port: port,
            txt: {
                version: "1.0.0",
                pid: process.pid.toString(),
                started: Date.now().toString()
            }
        });

        this.#advertisedService.on("up", () => {
            log(`Service "${serviceName}" is now advertised on port ${port}`);
        });

        this.#advertisedService.on("error", (err: Error) => {
            error("Service advertisement error:", err);
        });
    }

    /**
     * Start discovering other Konflikt services
     */
    startDiscovery(): void {
        debug("Starting service discovery for Konflikt services");

        this.#browser = this.#bonjour.find({ type: "konflikt" });

        this.#browser.on("up", (service: Service) => {
            const discoveredService: DiscoveredService = {
                name: service.name,
                host: service.host,
                port: service.port,
                addresses: service.addresses || [],
                txt: service.txt
            };

            // Don't discover ourselves
            if (service.txt?.pid === process.pid.toString()) {
                return;
            }

            this.#discoveredServices.set(service.name, discoveredService);

            log(`Discovered Konflikt service: ${service.name} at ${service.host}:${service.port}`);
            this.emit("serviceUp", discoveredService);
        });

        this.#browser.on("down", (service: Service) => {
            // Don't process our own service going down
            if (service.txt?.pid === process.pid.toString()) {
                return;
            }

            const discoveredService = this.#discoveredServices.get(service.name);
            if (discoveredService) {
                this.#discoveredServices.delete(service.name);
                log(`Lost Konflikt service: ${service.name}`);
                this.emit("serviceDown", discoveredService);
            }
        });

        this.#browser.start();
    }

    /**
     * Stop discovery and unpublish our service
     */
    async stop(): Promise<void> {
        debug("Stopping service discovery");

        if (this.#browser) {
            this.#browser.stop();
            this.#browser = undefined;
        }

        if (this.#advertisedService) {
            try {
                // Try different methods that might be available
                const service = this.#advertisedService as unknown as { stop?: () => void; unpublish?: () => void };
                if (typeof service.stop === "function") {
                    service.stop();
                } else if (typeof service.unpublish === "function") {
                    service.unpublish();
                }
            } catch (e) {
                debug("Error stopping advertised service:", e);
            }
            this.#advertisedService = undefined;
        }

        await new Promise<void>((resolve: () => void) => {
            this.#bonjour.destroy(() => {
                debug("Bonjour service destroyed");
                resolve();
            });
        });
    }

    /**
     * Get all currently discovered services
     */
    getDiscoveredServices(): DiscoveredService[] {
        return Array.from(this.#discoveredServices.values());
    }

    /**
     * Get a specific discovered service by name
     */
    getService(name: string): DiscoveredService | undefined {
        return this.#discoveredServices.get(name);
    }
}
