import type { PromiseData } from "./PromiseData";

export function createPromise<T = void>(): PromiseData<T> {
    let resolve: (value: T | PromiseLike<T>) => void;
    let reject: (reason: Error) => void;
    const promise = new Promise<T>((res: (value: T | PromiseLike<T>) => void, rej: (error: Error) => void) => {
        resolve = res;
        reject = rej;
    });
    return { resolve: resolve!, reject: reject!, promise };
}
