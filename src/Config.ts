import { EventEmitter } from "events";
import { debug } from "./Log";
import fs from "fs";

// interface ConfigEvents {
//     fileChanged: undefined;
// }

export class Config extends EventEmitter {
    constructor(filePath?: string) {
        super();
        try {
            if (filePath) {
                const config = JSON.parse(fs.readFileSync(filePath, "utf-8"));
                debug("Got config", config);
            }
        } catch (e: unknown) {
            throw new Error(`Failed to load config: ${e}`);
        }
    }
}
