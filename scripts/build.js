#!/usr/bin/env node

/**
 * Smart build script with dirty checking for all components.
 * Only rebuilds components whose source files have changed.
 */

const { execSync, spawn } = require("child_process");
const fs = require("fs");
const path = require("path");

const PROJECT_ROOT = path.resolve(__dirname, "..");

// Build state file to track what was built
const BUILD_STATE_FILE = path.join(PROJECT_ROOT, "dist/.build-state.json");

/**
 * Get all files matching extensions in a directory recursively
 */
function getFiles(dir, extensions, ignore = []) {
    const files = [];

    function walk(currentDir) {
        if (!fs.existsSync(currentDir)) return;

        const entries = fs.readdirSync(currentDir, { withFileTypes: true });
        for (const entry of entries) {
            const fullPath = path.join(currentDir, entry.name);
            const relativePath = path.relative(PROJECT_ROOT, fullPath);

            if (entry.isDirectory()) {
                if (
                    !entry.name.startsWith(".") &&
                    !ignore.some((i) => relativePath.startsWith(i) || entry.name === i)
                ) {
                    walk(fullPath);
                }
            } else if (entry.isFile()) {
                const ext = path.extname(entry.name);
                if (extensions.includes(ext)) {
                    files.push(fullPath);
                }
            }
        }
    }

    walk(dir);
    return files;
}

/**
 * Calculate a hash of file modification times
 */
function getSourceHash(files) {
    let hash = 0;
    for (const file of files.sort()) {
        if (fs.existsSync(file)) {
            const stat = fs.statSync(file);
            hash ^= stat.mtimeMs;
            hash = (hash << 5) - hash + file.length;
        }
    }
    return hash.toString(16);
}

/**
 * Load build state
 */
function loadBuildState() {
    try {
        if (fs.existsSync(BUILD_STATE_FILE)) {
            return JSON.parse(fs.readFileSync(BUILD_STATE_FILE, "utf8"));
        }
    } catch {
        // Ignore errors
    }
    return {};
}

/**
 * Save build state
 */
function saveBuildState(state) {
    const dir = path.dirname(BUILD_STATE_FILE);
    if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
    }
    fs.writeFileSync(BUILD_STATE_FILE, JSON.stringify(state, null, 2));
}

/**
 * Run a command and stream output
 */
function runCommand(cmd, options = {}) {
    return new Promise((resolve, reject) => {
        console.log(`  → ${cmd}`);
        const proc = spawn(cmd, [], {
            stdio: "inherit",
            shell: true,
            cwd: PROJECT_ROOT,
            ...options,
        });

        proc.on("close", (code) => {
            if (code === 0) {
                resolve();
            } else {
                reject(new Error(`Command failed with code ${code}`));
            }
        });

        proc.on("error", reject);
    });
}

/**
 * Check if a component needs rebuilding
 */
function needsRebuild(state, component, currentHash) {
    return state[component] !== currentHash;
}

/**
 * Build TypeScript
 */
async function buildTypeScript(state, force) {
    const sources = getFiles(path.join(PROJECT_ROOT, "src/app"), [".ts"], ["node_modules"]);
    const configFile = path.join(PROJECT_ROOT, "tsconfig.json");
    if (fs.existsSync(configFile)) sources.push(configFile);

    const hash = getSourceHash(sources);

    if (!force && !needsRebuild(state, "typescript", hash)) {
        console.log("✓ TypeScript is up to date");
        return hash;
    }

    console.log("Building TypeScript...");
    await runCommand("npx tsc");
    console.log("✓ TypeScript build complete");
    return hash;
}

/**
 * Build native addon
 */
async function buildNative(state, force, config = "Release") {
    const sources = getFiles(path.join(PROJECT_ROOT, "src/native"), [".cpp", ".h", ".txt"], [
        "node_modules",
    ]);

    const hash = getSourceHash(sources);
    const stateKey = `native-${config.toLowerCase()}`;

    if (!force && !needsRebuild(state, stateKey, hash)) {
        console.log(`✓ Native (${config}) is up to date`);
        return hash;
    }

    console.log(`Building native addon (${config})...`);
    const outDir = `dist/native/${config}`;
    await runCommand(
        `npx cmake-js compile -l error --directory src/native --out ${outDir} --config ${config}`
    );
    console.log(`✓ Native (${config}) build complete`);
    return hash;
}

/**
 * Build web UI
 */
async function buildUI(state, force) {
    const sources = getFiles(path.join(PROJECT_ROOT, "src/webpage"), [".ts", ".tsx", ".css", ".html"], [
        "node_modules",
        "dist",
    ]);
    const configFiles = ["vite.config.ts", "tsconfig.json", "package.json"].map((f) =>
        path.join(PROJECT_ROOT, "src/webpage", f)
    );
    for (const f of configFiles) {
        if (fs.existsSync(f)) sources.push(f);
    }

    const hash = getSourceHash(sources);

    if (!force && !needsRebuild(state, "ui", hash)) {
        console.log("✓ Web UI is up to date");
        return hash;
    }

    console.log("Building web UI...");
    await runCommand("npm run build", { cwd: path.join(PROJECT_ROOT, "src/webpage") });
    console.log("✓ Web UI build complete");
    return hash;
}

/**
 * Build macOS app
 */
async function buildMacOS(state, force, options = {}) {
    if (process.platform !== "darwin") {
        console.log("⊘ Skipping macOS build (not on macOS)");
        return null;
    }

    const { universal = false, bundleBackend = false } = options;

    const sourceDir = path.join(PROJECT_ROOT, "src/desktop/macos");
    const sources = getFiles(sourceDir, [".swift", ".plist", ".entitlements", ".sh"], []);

    const hash = getSourceHash(sources);
    const stateKey = `macos${universal ? "-universal" : ""}`;

    if (!force && !needsRebuild(state, stateKey, hash)) {
        console.log("✓ macOS app is up to date");
        return hash;
    }

    console.log("Building macOS app...");

    const buildScript = path.join(sourceDir, "build.sh");
    fs.chmodSync(buildScript, 0o755);

    const args = [];
    if (universal) args.push("--universal");
    if (bundleBackend) args.push("--bundle-backend");

    await runCommand(`./build.sh ${args.join(" ")}`, { cwd: sourceDir });
    console.log("✓ macOS build complete");
    return hash;
}

/**
 * Run linting
 */
async function runLint(skipLint) {
    if (skipLint) {
        console.log("⊘ Skipping lint (--no-lint)");
        return;
    }

    console.log("Running lint...");
    await runCommand("npm run lint");
    console.log("✓ Lint passed");
}

/**
 * Post-build steps
 */
async function postBuild() {
    const indexJs = path.join(PROJECT_ROOT, "dist/app/index.js");
    if (fs.existsSync(indexJs)) {
        fs.chmodSync(indexJs, 0o755);
    }

    const binDir = path.join(PROJECT_ROOT, "bin");
    if (!fs.existsSync(binDir)) {
        fs.mkdirSync(binDir, { recursive: true });
    }

    const binLink = path.join(binDir, "konflikt");
    const target = "../dist/app/index.js";
    try {
        if (fs.existsSync(binLink)) {
            fs.unlinkSync(binLink);
        }
        fs.symlinkSync(target, binLink);
    } catch {
        // Symlink might fail on Windows
    }
}

/**
 * Print help message
 */
function printHelp() {
    console.log(`
Konflikt Build System

Usage: node scripts/build.js [options]

Options:
  --force, -f        Force rebuild all components
  --no-lint          Skip linting
  --no-native        Skip native addon build
  --no-ui            Skip web UI build
  --no-desktop       Skip desktop app build
  --desktop-only     Only build desktop apps
  --universal        Build universal binary (macOS only)
  --bundle-backend   Bundle Node.js backend in app (macOS only)
  --debug-only       Only build debug native addon
  --release-only     Only build release native addon
  --help, -h         Show this help message

Examples:
  npm run build                    # Build everything (with dirty checking)
  npm run build:force              # Force rebuild everything
  npm run build -- --no-lint       # Build without linting
  npm run build:macos:universal    # Build macOS universal binary
`);
}

/**
 * Main entry point
 */
async function main() {
    const args = process.argv.slice(2);

    if (args.includes("--help") || args.includes("-h")) {
        printHelp();
        return;
    }

    const options = {
        force: args.includes("--force") || args.includes("-f"),
        skipLint: args.includes("--no-lint"),
        skipNative: args.includes("--no-native"),
        skipUI: args.includes("--no-ui"),
        skipDesktop: args.includes("--no-desktop"),
        universal: args.includes("--universal"),
        bundleBackend: args.includes("--bundle-backend"),
        debugOnly: args.includes("--debug-only"),
        releaseOnly: args.includes("--release-only"),
    };

    // Handle specific component builds
    if (args.includes("--desktop-only")) {
        const state = loadBuildState();
        const hash = await buildMacOS(state, options.force, options);
        if (hash) {
            state[`macos${options.universal ? "-universal" : ""}`] = hash;
            saveBuildState(state);
        }
        return;
    }

    console.log("╔════════════════════════════════════════╗");
    console.log("║         Konflikt Build System          ║");
    console.log("╚════════════════════════════════════════╝");
    console.log();

    const state = loadBuildState();
    const newState = { ...state };

    try {
        // Lint first
        await runLint(options.skipLint);
        console.log();

        // Build native addon (in parallel if both configs needed)
        if (!options.skipNative) {
            const builds = [];
            if (!options.debugOnly) {
                builds.push(
                    buildNative(state, options.force, "Release").then((h) => {
                        if (h) newState["native-release"] = h;
                    })
                );
            }
            if (!options.releaseOnly) {
                builds.push(
                    buildNative(state, options.force, "Debug").then((h) => {
                        if (h) newState["native-debug"] = h;
                    })
                );
            }
            await Promise.all(builds);
            console.log();
        }

        // Build TypeScript
        const tsHash = await buildTypeScript(state, options.force);
        if (tsHash) newState["typescript"] = tsHash;
        console.log();

        // Build UI
        if (!options.skipUI) {
            const uiHash = await buildUI(state, options.force);
            if (uiHash) newState["ui"] = uiHash;
            console.log();
        }

        // Build desktop apps
        if (!options.skipDesktop) {
            const macosHash = await buildMacOS(state, options.force, options);
            if (macosHash) {
                newState[`macos${options.universal ? "-universal" : ""}`] = macosHash;
            }
            console.log();
        }

        // Post-build
        await postBuild();

        // Save state
        saveBuildState(newState);

        console.log("════════════════════════════════════════");
        console.log("Build completed successfully!");
        console.log("════════════════════════════════════════");
    } catch (error) {
        console.error("\n✗ Build failed:", error.message);
        process.exit(1);
    }
}

main();
