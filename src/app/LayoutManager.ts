import { EventEmitter } from "events";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "fs";
import { homedir } from "os";
import { join } from "path";
import { verbose } from "./verbose";
import type { Adjacency } from "./Adjacency";
import type { ScreenEntry } from "./ScreenEntry";

interface LayoutManagerEvents {
    layoutChanged: [screens: ScreenEntry[]];
}

const EDGE_TOLERANCE = 10; // pixels tolerance for adjacency detection

export class LayoutManager extends EventEmitter<LayoutManagerEvents> {
    #screens: Map<string, ScreenEntry> = new Map();
    #configPath: string;
    #serverScreen: ScreenEntry | null = null;

    constructor(configPath?: string) {
        super();
        this.#configPath = configPath ?? join(homedir(), ".config", "konflikt", "layout.json");
        this.loadFromConfig();
    }

    /**
     * Set the server's own screen info
     */
    setServerScreen(
        instanceId: string,
        displayName: string,
        machineId: string,
        width: number,
        height: number
    ): ScreenEntry {
        this.#serverScreen = {
            instanceId,
            displayName,
            machineId,
            x: 0,
            y: 0,
            width,
            height,
            isServer: true,
            online: true
        };
        this.#screens.set(instanceId, this.#serverScreen);
        this.#emitLayoutChanged();
        return this.#serverScreen;
    }

    /**
     * Register a new client. Returns the assigned screen entry with position.
     */
    registerClient(
        instanceId: string,
        displayName: string,
        machineId: string,
        width: number,
        height: number
    ): ScreenEntry {
        // Check if this client was previously registered (has saved position)
        const existingEntry = this.#screens.get(instanceId);

        if (existingEntry) {
            // Reconnecting client - preserve position, update online status
            existingEntry.displayName = displayName;
            existingEntry.machineId = machineId;
            existingEntry.width = width;
            existingEntry.height = height;
            existingEntry.online = true;
            this.#emitLayoutChanged();
            return existingEntry;
        }

        // New client - auto-arrange position
        const position = this.autoArrangeNewClient(width, height);

        const entry: ScreenEntry = {
            instanceId,
            displayName,
            machineId,
            x: position.x,
            y: position.y,
            width,
            height,
            isServer: false,
            online: true
        };

        this.#screens.set(instanceId, entry);
        this.persistToConfig();
        this.#emitLayoutChanged();
        return entry;
    }

    /**
     * Unregister a client (mark as offline, but preserve position)
     */
    unregisterClient(instanceId: string): void {
        const entry = this.#screens.get(instanceId);
        if (entry && !entry.isServer) {
            entry.online = false;
            this.#emitLayoutChanged();
        }
    }

    /**
     * Permanently remove a client from layout
     */
    removeClient(instanceId: string): void {
        const entry = this.#screens.get(instanceId);
        if (entry && !entry.isServer) {
            this.#screens.delete(instanceId);
            this.persistToConfig();
            this.#emitLayoutChanged();
        }
    }

    /**
     * Update a screen's position
     */
    updatePosition(instanceId: string, x: number, y: number): void {
        const entry = this.#screens.get(instanceId);
        if (entry) {
            entry.x = x;
            entry.y = y;
            this.persistToConfig();
            this.#emitLayoutChanged();
        }
    }

    /**
     * Get all screens in the layout
     */
    getLayout(): ScreenEntry[] {
        return Array.from(this.#screens.values());
    }

    /**
     * Get a specific screen entry
     */
    getScreen(instanceId: string): ScreenEntry | undefined {
        return this.#screens.get(instanceId);
    }

    /**
     * Calculate adjacency for a specific screen based on positions
     */
    getAdjacencyFor(instanceId: string): Adjacency {
        const screen = this.#screens.get(instanceId);
        if (!screen) {
            return {};
        }

        const adjacency: Adjacency = {};

        for (const [otherId, other] of this.#screens) {
            if (otherId === instanceId || !other.online) {
                continue;
            }

            // Check left adjacency: other's right edge touches this screen's left edge
            if (
                LayoutManager.#edgesTouch(other.x + other.width, screen.x) &&
                LayoutManager.#verticalOverlap(screen, other)
            ) {
                adjacency.left = otherId;
            }

            // Check right adjacency: other's left edge touches this screen's right edge
            if (
                LayoutManager.#edgesTouch(screen.x + screen.width, other.x) &&
                LayoutManager.#verticalOverlap(screen, other)
            ) {
                adjacency.right = otherId;
            }

            // Check top adjacency: other's bottom edge touches this screen's top edge
            if (
                LayoutManager.#edgesTouch(other.y + other.height, screen.y) &&
                LayoutManager.#horizontalOverlap(screen, other)
            ) {
                adjacency.top = otherId;
            }

            // Check bottom adjacency: other's top edge touches this screen's bottom edge
            if (
                LayoutManager.#edgesTouch(screen.y + screen.height, other.y) &&
                LayoutManager.#horizontalOverlap(screen, other)
            ) {
                adjacency.bottom = otherId;
            }
        }

        return adjacency;
    }

    /**
     * Auto-arrange a new client: place it to the right of existing screens
     */
    autoArrangeNewClient(_width: number, _height: number): { x: number; y: number } {
        let maxRight = 0;

        for (const screen of this.#screens.values()) {
            const right = screen.x + screen.width;
            if (right > maxRight) {
                maxRight = right;
            }
        }

        return { x: maxRight, y: 0 };
    }

    /**
     * Persist current layout to config file
     */
    persistToConfig(): void {
        try {
            const layoutData = {
                version: 1,
                screens: Array.from(this.#screens.values())
                    .filter((s: ScreenEntry) => !s.isServer) // Don't persist server screen position
                    .map((s: ScreenEntry) => ({
                        instanceId: s.instanceId,
                        displayName: s.displayName,
                        machineId: s.machineId,
                        x: s.x,
                        y: s.y,
                        width: s.width,
                        height: s.height
                    }))
            };

            // Ensure directory exists
            const dir = this.#configPath.substring(0, this.#configPath.lastIndexOf("/"));
            if (!existsSync(dir)) {
                mkdirSync(dir, { recursive: true });
            }

            writeFileSync(this.#configPath, JSON.stringify(layoutData, null, 2));
            verbose(`Layout persisted to ${this.#configPath}`);
        } catch (err) {
            verbose(`Failed to persist layout: ${err}`);
        }
    }

    /**
     * Load layout from config file
     */
    loadFromConfig(): void {
        try {
            if (!existsSync(this.#configPath)) {
                verbose(`No layout config found at ${this.#configPath}`);
                return;
            }

            const data = readFileSync(this.#configPath, "utf8");
            const layoutData = JSON.parse(data) as {
                version: number;
                screens: Array<{
                    instanceId: string;
                    displayName: string;
                    machineId: string;
                    x: number;
                    y: number;
                    width: number;
                    height: number;
                }>;
            };

            for (const screenData of layoutData.screens) {
                const entry: ScreenEntry = {
                    ...screenData,
                    isServer: false,
                    online: false // Will be set to true when client connects
                };
                this.#screens.set(screenData.instanceId, entry);
            }

            verbose(`Loaded ${layoutData.screens.length} screens from layout config`);
        } catch (err) {
            verbose(`Failed to load layout config: ${err}`);
        }
    }

    /**
     * Update the entire layout (for UI drag-drop operations)
     */
    updateLayout(screens: Array<{ instanceId: string; x: number; y: number }>): void {
        for (const update of screens) {
            const entry = this.#screens.get(update.instanceId);
            if (entry) {
                entry.x = update.x;
                entry.y = update.y;
            }
        }
        this.persistToConfig();
        this.#emitLayoutChanged();
    }

    static #edgesTouch(edge1: number, edge2: number): boolean {
        return Math.abs(edge1 - edge2) <= EDGE_TOLERANCE;
    }

    static #verticalOverlap(a: ScreenEntry, b: ScreenEntry): boolean {
        // Check if screens overlap vertically (have some shared Y range)
        return a.y < b.y + b.height && a.y + a.height > b.y;
    }

    static #horizontalOverlap(a: ScreenEntry, b: ScreenEntry): boolean {
        // Check if screens overlap horizontally (have some shared X range)
        return a.x < b.x + b.width && a.x + a.width > b.x;
    }

    #emitLayoutChanged(): void {
        this.emit("layoutChanged", this.getLayout());
    }
}
