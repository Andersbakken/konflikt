import type { ConsoleErrorMessage } from "./ConsoleErrorMessage";
import type { ConsoleLogMessage } from "./ConsoleLogMessage";
import type { ConsolePongMessage } from "./ConsolePongMessage";
import type { ConsoleResponseMessage } from "./ConsoleResponseMessage";

export type ConsoleMessage = ConsoleResponseMessage | ConsoleErrorMessage | ConsolePongMessage | ConsoleLogMessage;
