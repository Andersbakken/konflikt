import { execSync } from "child_process";

let cachedCommit: string | undefined;

export function getGitCommit(): string | undefined {
    if (cachedCommit !== undefined) {
        return cachedCommit;
    }

    try {
        cachedCommit = execSync("git rev-parse HEAD", {
            encoding: "utf-8",
            stdio: ["pipe", "pipe", "pipe"]
        }).trim();
        return cachedCommit;
    } catch {
        return undefined;
    }
}
