import { existsSync, mkdirSync, readFileSync, writeFileSync } from "fs";
import { homedir } from "os";
import { join } from "path";
import { randomUUID } from "crypto";

const CLIENT_ID_PATH = join(homedir(), ".config", "konflikt", "client-id");

/**
 * Get or create a persistent client ID.
 * The ID is stored in ~/.config/konflikt/client-id and persists across restarts.
 */
export function getPersistentClientId(): string {
    // Check if client ID already exists
    if (existsSync(CLIENT_ID_PATH)) {
        try {
            const id = readFileSync(CLIENT_ID_PATH, "utf8").trim();
            if (id) {
                return id;
            }
        } catch {
            // Fall through to generate new ID
        }
    }

    // Generate new UUID
    const id = randomUUID();

    // Ensure directory exists
    const dir = join(homedir(), ".config", "konflikt");
    if (!existsSync(dir)) {
        mkdirSync(dir, { recursive: true });
    }

    // Save the ID
    writeFileSync(CLIENT_ID_PATH, id, "utf8");

    return id;
}
