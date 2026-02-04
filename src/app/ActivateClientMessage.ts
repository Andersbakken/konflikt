export interface ActivateClientMessage {
    type: "activate_client";
    targetInstanceId: string;
    cursorX: number;
    cursorY: number;
    timestamp: number;
}
