import type { ClientRegistrationMessage } from "./ClientRegistrationMessage";
import type { ConsoleMessage } from "./ConsoleMessage";
import type { InputEventMessage } from "./InputEventMessage";
import type { InstanceInfoMessage } from "./InstanceInfoMessage";
import type { LayoutAssignmentMessage } from "./LayoutAssignmentMessage";
import type { LayoutUpdateMessage } from "./LayoutUpdateMessage";

export type NetworkMessage =
    | ClientRegistrationMessage
    | ConsoleMessage
    | InputEventMessage
    | InstanceInfoMessage
    | LayoutAssignmentMessage
    | LayoutUpdateMessage;
