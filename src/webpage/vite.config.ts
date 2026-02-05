import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
    plugins: [react()],
    base: "/ui/",
    resolve: {
        // Resolve modules from symlink location, not target - needed for out-of-source builds
        preserveSymlinks: true
    },
    build: {
        outDir: "../../dist/ui",
        emptyOutDir: true
    },
    server: {
        port: 5173,
        proxy: {
            "/api": {
                target: "http://localhost:3000",
                changeOrigin: true
            },
            "/ws": {
                target: "ws://localhost:3000",
                ws: true
            }
        }
    }
});
