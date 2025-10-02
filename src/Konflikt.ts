import fs from 'fs';

export class Konflikt {
    constructor(configPath?: string) {
        try {
            if (configPath) {
                const config = JSON.parse(fs.readFileSync(configPath, 'utf-8'));
            }
        } catch (e: unknown) {
            throw new Error(`Failed to load config: ${e}`);
        }
    }

    // should auto-discover peers
    async init(): Promise<void> {}
}
