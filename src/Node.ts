import { NodeType } from "./NodeType";

export interface Node {
    address: string | undefined;
    name: string | undefined;
    port: number | undefined;
    type: NodeType;
}
