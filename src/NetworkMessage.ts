import type { ConsoleMessage } from "./ConsoleMessage";
import type { InputEventMessage } from "./InputEventMessage";
import type { InstanceInfoMessage } from "./InstanceInfoMessage";

export type NetworkMessage = ConsoleMessage | InputEventMessage | InstanceInfoMessage;
