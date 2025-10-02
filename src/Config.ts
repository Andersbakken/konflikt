import { EventEmitter } from "events";
import fs from "fs";

// interface ConfigEvents {
//     fileChanged: undefined;
// }

export class Config extends EventEmitter {
    #server?:

    constructor(filePath?: string) {
        super();
        try {
            if (filePath) {
                const config = JSON.parse(fs.readFileSync(filePath, 'utf-8'));
            }
        } catch (e: unknown) {
            throw new Error(`Failed to load config: ${e}`);
        }
    }
}
