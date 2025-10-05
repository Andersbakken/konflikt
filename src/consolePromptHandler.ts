let consolePromptHandler: (() => void) | undefined;

export function setConsolePromptHandler(handler: (() => void) | undefined): void {
    consolePromptHandler = handler;
}

export function getConsolePromptHandler(): (() => void) | undefined {
    return consolePromptHandler;
}