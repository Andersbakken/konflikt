import { isConsoleMessage } from "./isConsoleMessage";
import { isInputEventMessage } from "./isInputEventMessage";
import { isInstanceInfoMessage } from "./isInstanceInfoMessage";
import type { NetworkMessage } from "./NetworkMessage";

export function isNetworkMessage(obj: unknown): obj is NetworkMessage {
    return isInputEventMessage(obj) || isConsoleMessage(obj) || isInstanceInfoMessage(obj);
}
