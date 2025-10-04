import * as convict from "convict";
import { format } from "util";
// Note: convict-format-with-validator has issues with ES modules, using basic convict for now

// Centralized CLI argument definitions - single source of truth
const CLI_ARGS = {
    "instance-id": { path: "instance.id" },
    "instance-name": { path: "instance.name" },
    role: { path: "instance.role", short: "-r" },
    port: { path: "network.port", short: "-p" },
    host: { path: "network.host", short: "-H" },
    discovery: { path: "network.discovery.enabled" },
    "service-name": { path: "network.discovery.serviceName", short: "-s" },
    "screen-id": { path: "screen.id" },
    "screen-x": { path: "screen.position.x" },
    "screen-y": { path: "screen.position.y" },
    "screen-width": { path: "screen.dimensions.width" },
    "screen-height": { path: "screen.dimensions.height" },
    "screen-edges": { path: "screen.edges" },
    topology: { path: "cluster.topology", short: "-t" },
    "server-host": { path: "cluster.server.host" },
    "server-port": { path: "cluster.server.port" },
    peers: { path: "cluster.peers" },
    adjacency: { path: "cluster.adjacency" },
    "capture-mouse": { path: "input.capture.mouse" },
    "capture-keyboard": { path: "input.capture.keyboard" },
    "forward-events": { path: "input.forward" },
    "cursor-transition": { path: "input.cursorTransition.enabled" },
    "dead-zone": { path: "input.cursorTransition.deadZone" },
    "log-level": { path: "logging.level", short: "-l" },
    "log-file": { path: "logging.file", short: "-f" },
    dev: { path: "development.enabled", short: "-d" },
    "mock-input": { path: "development.mockInput" },
    "console": { path: "console.enabled" }
} as const;

// Generate short option mappings from CLI_ARGS
export const SHORT_OPTIONS: Record<string, string> = Object.fromEntries(
    Object.entries(CLI_ARGS)
        .filter(([, config]: [string, { path: string; short?: string }]) => config.short)
        .map(([longArg, config]: [string, { path: string; short?: string }]) => [config.short!, `--${longArg}`])
);

// Helper function to get the arg name for a config path
function argForPath(configPath: string): string | undefined {
    const entry: [string, { path: string; short?: string }] | undefined = Object.entries(CLI_ARGS).find(
        ([, config]: [string, { path: string; short?: string }]) => config.path === configPath
    );
    return entry ? entry[0] : undefined;
}

// Add custom formats for nullable numbers
convict.addFormat({
    name: "nullable-port",
    validate: (val: unknown) => {
        if (val === null) {
            return;
        }
        if (typeof val !== "number" || val < 0 || val > 65535 || !Number.isInteger(val)) {
            throw new Error("must be null or a port number (0-65535)");
        }
    },
    coerce: (val: unknown) => {
        if (val === null) {
            return null;
        }
        return Number(val);
    }
});

convict.addFormat({
    name: "nullable-nat",
    validate: (val: unknown) => {
        if (val === null) {
            return;
        }
        if (typeof val !== "number" || val < 0 || !Number.isInteger(val)) {
            throw new Error("must be null or a positive integer");
        }
    },
    coerce: (val: unknown) => {
        if (val === null) {
            return null;
        }
        return Number(val);
    }
});

convict.addFormat({
    name: "nullable-string",
    validate: (val: unknown): void => {
        if (val === null) {
            return;
        }
        if (typeof val !== "string") {
            throw new Error("must be null or a string");
        }
    },
    coerce: (val: unknown): unknown => {
        if (val === null) {
            return null;
        }
        if (typeof val === "string") {
            return val;
        }
        return format(val);
    }
});

// Define the configuration schema
export const configSchema = convict.default({
    // Instance identity
    instance: {
        id: {
            doc: "Unique instance identifier (auto-generated if not provided)",
            format: String,
            default: null,
            env: "KONFLIKT_INSTANCE_ID",
            arg: argForPath("instance.id")
        },
        name: {
            doc: "Human-readable instance name",
            format: String,
            default: null,
            env: "KONFLIKT_INSTANCE_NAME",
            arg: argForPath("instance.name")
        },
        role: {
            doc: "Instance role - client connects to servers and sends input, server receives input from clients",
            format: ["server", "client"],
            default: "client",
            env: "KONFLIKT_ROLE",
            arg: argForPath("instance.role")
        }
    },

    // Network configuration
    network: {
        port: {
            doc: "WebSocket server port",
            format: "port",
            default: 3000,
            env: "KONFLIKT_PORT",
            arg: argForPath("network.port")
        },
        host: {
            doc: "Host to bind server to",
            format: String,
            default: "0.0.0.0",
            env: "KONFLIKT_HOST",
            arg: argForPath("network.host")
        },
        discovery: {
            enabled: {
                doc: "Enable service discovery via mDNS/Bonjour",
                format: Boolean,
                default: true,
                env: "KONFLIKT_DISCOVERY_ENABLED",
                arg: argForPath("network.discovery.enabled")
            },
            serviceName: {
                doc: "Service name for mDNS advertising",
                format: String,
                default: "konflikt",
                env: "KONFLIKT_SERVICE_NAME",
                arg: argForPath("network.discovery.serviceName")
            }
        }
    },

    // Physical screen layout
    screen: {
        id: {
            doc: "Unique screen identifier",
            format: String,
            default: null,
            env: "KONFLIKT_SCREEN_ID",
            arg: argForPath("screen.id")
        },
        position: {
            x: {
                doc: "Screen X position in virtual coordinate space",
                format: Number,
                default: 0,
                env: "KONFLIKT_SCREEN_X",
                arg: argForPath("screen.position.x")
            },
            y: {
                doc: "Screen Y position in virtual coordinate space",
                format: Number,
                default: 0,
                env: "KONFLIKT_SCREEN_Y",
                arg: argForPath("screen.position.y")
            }
        },
        dimensions: {
            width: {
                doc: "Screen width in pixels (auto-detected if not set)",
                format: "nullable-nat",
                default: null,
                env: "KONFLIKT_SCREEN_WIDTH",
                arg: argForPath("screen.dimensions.width")
            },
            height: {
                doc: "Screen height in pixels (auto-detected if not set)",
                format: "nullable-nat",
                default: null,
                env: "KONFLIKT_SCREEN_HEIGHT",
                arg: argForPath("screen.dimensions.height")
            }
        },
        edges: {
            doc: "Which screen edges allow cursor transitions",
            format: Object,
            default: {
                top: true,
                right: true,
                bottom: true,
                left: true
            },
            env: "KONFLIKT_SCREEN_EDGES",
            arg: argForPath("screen.edges")
        }
    },

    // Cluster topology - defines relationships between instances
    cluster: {
        topology: {
            doc: "Cluster topology mode",
            format: ["automatic", "manual", "star", "mesh"],
            default: "automatic",
            env: "KONFLIKT_TOPOLOGY",
            arg: argForPath("cluster.topology")
        },

        // Server configuration (for star topology)
        server: {
            host: {
                doc: "Server host to connect to (for client mode)",
                format: "nullable-string",
                default: null,
                env: "KONFLIKT_SERVER_HOST",
                arg: argForPath("cluster.server.host")
            },
            port: {
                doc: "Server port to connect to (for client mode)",
                format: "nullable-port",
                default: null,
                env: "KONFLIKT_SERVER_PORT",
                arg: argForPath("cluster.server.port")
            }
        },

        // Manual peer definitions
        peers: {
            doc: "Manually defined peer connections",
            format: Array,
            default: [],
            env: "KONFLIKT_PEERS",
            arg: argForPath("cluster.peers")
        },

        // Screen adjacency map (which screens are next to each other)
        adjacency: {
            doc: "Screen adjacency definitions for cursor flow",
            format: Object,
            default: {},
            env: "KONFLIKT_ADJACENCY",
            arg: argForPath("cluster.adjacency")
        }
    },

    // Input handling
    input: {
        capture: {
            mouse: {
                doc: "Capture mouse events",
                format: Boolean,
                default: true,
                env: "KONFLIKT_CAPTURE_MOUSE",
                arg: argForPath("input.capture.mouse")
            },
            keyboard: {
                doc: "Capture keyboard events",
                format: Boolean,
                default: true,
                env: "KONFLIKT_CAPTURE_KEYBOARD",
                arg: argForPath("input.capture.keyboard")
            }
        },
        forward: {
            doc: "Which event types to forward to peers",
            format: Array,
            default: ["mouse_move", "mouse_press", "mouse_release", "key_press", "key_release"],
            env: "KONFLIKT_FORWARD_EVENTS",
            arg: argForPath("input.forward")
        },
        cursorTransition: {
            enabled: {
                doc: "Enable automatic cursor transition between screens",
                format: Boolean,
                default: true,
                env: "KONFLIKT_CURSOR_TRANSITION",
                arg: argForPath("input.cursorTransition.enabled")
            },
            deadZone: {
                doc: "Dead zone in pixels at screen edges before transition",
                format: "nat",
                default: 5,
                env: "KONFLIKT_DEAD_ZONE",
                arg: argForPath("input.cursorTransition.deadZone")
            }
        }
    },

    // Logging configuration
    logging: {
        level: {
            doc: "Log level",
            format: ["silent", "error", "log", "debug", "verbose"],
            default: "log",
            env: "KONFLIKT_LOG_LEVEL",
            arg: argForPath("logging.level")
        },
        file: {
            doc: "Log file path",
            format: "nullable-string",
            default: null,
            env: "KONFLIKT_LOG_FILE",
            arg: argForPath("logging.file")
        }
    },

    // Development/debugging
    development: {
        enabled: {
            doc: "Enable development mode",
            format: Boolean,
            default: false,
            env: "NODE_ENV",
            arg: argForPath("development.enabled")
        },
        mockInput: {
            doc: "Mock input events for testing",
            format: Boolean,
            default: false,
            env: "KONFLIKT_MOCK_INPUT",
            arg: argForPath("development.mockInput")
        }
    },

    // Console configuration
    console: {
        enabled: {
            doc: "Console mode: true=enabled, false=disabled, or host[:port] for remote console",
            format: "nullable-string",
            default: "true",
            env: "KONFLIKT_CONSOLE",
            arg: argForPath("console.enabled")
        }
    }
});

export type ConfigType = ReturnType<typeof configSchema.getProperties>;
