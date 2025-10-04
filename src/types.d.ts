// Type declarations for modules without type definitions

declare module 'convict-format-with-validator' {
    import type { Config } from 'convict';
    
    export function addFormats(convict: typeof Config): void;
}