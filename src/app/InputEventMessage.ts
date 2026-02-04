import type { InputEventData } from "./InputEventData";
import type { InputEventType } from "./InputEventType";

export interface InputEventMessage {
    type: "input_event";
    sourceInstanceId: string;
    sourceDisplayId: string;
    sourceMachineId: string;
    eventType: InputEventType;
    eventData: InputEventData;
}
