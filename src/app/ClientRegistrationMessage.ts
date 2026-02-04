export interface ClientRegistrationMessage {
    type: "client_registration";
    instanceId: string;
    displayName: string;
    machineId: string;
    screenWidth: number;
    screenHeight: number;
}
