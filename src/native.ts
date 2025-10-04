import type { KonfliktNative as IKonfliktNative, NativeLoggerCallbacks } from "./KonfliktNative.js";

// Determine which build to load based on NODE_ENV or KONFLIKT_BUILD
const buildType =
    process.env.KONFLIKT_BUILD === "debug" || process.env.NODE_ENV === "development" ? "Debug" : "Release";

// Load the native module
// eslint-disable-next-line @typescript-eslint/no-require-imports
const nativeModule = require(`build/${buildType}/konflikt_native.node`) as {
    KonfliktNative: new (logger?: NativeLoggerCallbacks) => IKonfliktNative;
};

export const { KonfliktNative } = nativeModule;
/* eslint-disable-next-line @typescript-eslint/no-redeclare, no-redeclare */
export type KonfliktNative = IKonfliktNative;
