import type { Point } from "./Point";

export class Rect {
    constructor(
        public x: number,
        public y: number,
        public width: number,
        public height: number
    ) {}

    get right(): number {
        return this.x + this.width;
    }

    set right(value: number) {
        if (value < this.x) {
            throw new Error("Right edge cannot be less than left edge");
        }
        const wid = value - this.x;
        this.width = wid;
    }

    get bottom(): number {
        return this.y + this.height;
    }

    set bottom(value: number) {
        if (value < this.y) {
            throw new Error("Bottom edge cannot be less than top edge");
        }
        const hei = value - this.y;
        this.height = hei;
    }

    get left(): number {
        return this.x;
    }

    set left(value: number) {
        this.x = value;
    }

    get top(): number {
        return this.y;
    }

    set top(value: number) {
        this.y = value;
    }

    contains(x: number, y: number): boolean;
    contains(point: Point): boolean;
    contains(arg1: number | Point, arg2?: number): boolean {
        let point: Point;
        if (typeof arg1 === "number") {
            if (typeof arg2 !== "number") {
                throw new Error("Y coordinate must be provided when X is a number");
            }
            point = { x: arg1, y: arg2 };
        } else {
            point = arg1;
        }
        return point.x >= this.left && point.x < this.right && point.y >= this.top && point.y < this.bottom;
    }

    clone(): Rect {
        return new Rect(this.x, this.y, this.width, this.height);
    }
}
