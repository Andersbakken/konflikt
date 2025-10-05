export function isEADDRINUSE(err: unknown): boolean {
    return typeof err === "object" && err !== null && "code" in err && err.code === "EADDRINUSE";
}
