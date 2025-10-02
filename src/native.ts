// Load the native module - types are declared globally in KonfliktNative.d.ts
// eslint-disable-next-line @typescript-eslint/no-require-imports
const nativeModule = require("../build/Release/konflikt_native.node") as {
    KonfliktNative: new () => KonfliktNative;
};

export { nativeModule };
export const { KonfliktNative } = nativeModule;
