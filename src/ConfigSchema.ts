import convict from "convict";
// Note: convict-format-with-validator has issues with ES modules, using basic convict for now

// Define the configuration schema
export const configSchema = convict({
    // Instance identity
    instance: {
        id: {
            doc: "Unique instance identifier (auto-generated if not provided)",
            format: String,
            default: null,
            env: "KONFLIKT_INSTANCE_ID",
            arg: "instance-id"
        },
        name: {
            doc: "Human-readable instance name",
            format: String,
            default: null,
            env: "KONFLIKT_INSTANCE_NAME", 
            arg: "instance-name"
        },
        role: {
            doc: "Instance role in the cluster",
            format: ["server", "client", "peer"],
            default: "peer",
            env: "KONFLIKT_ROLE",
            arg: "role"
        }
    },

    // Network configuration
    network: {
        port: {
            doc: "WebSocket server port",
            format: "port",
            default: 3000,
            env: "KONFLIKT_PORT",
            arg: "port"
        },
        host: {
            doc: "Host to bind server to",
            format: String,
            default: "0.0.0.0",
            env: "KONFLIKT_HOST",
            arg: "host"
        },
        discovery: {
            enabled: {
                doc: "Enable service discovery via mDNS/Bonjour",
                format: Boolean,
                default: true,
                env: "KONFLIKT_DISCOVERY_ENABLED",
                arg: "discovery"
            },
            serviceName: {
                doc: "Service name for mDNS advertising",
                format: String,
                default: "konflikt",
                env: "KONFLIKT_SERVICE_NAME",
                arg: "service-name"
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
            arg: "screen-id"
        },
        position: {
            x: {
                doc: "Screen X position in virtual coordinate space",
                format: Number,
                default: 0,
                env: "KONFLIKT_SCREEN_X",
                arg: "screen-x"
            },
            y: {
                doc: "Screen Y position in virtual coordinate space", 
                format: Number,
                default: 0,
                env: "KONFLIKT_SCREEN_Y",
                arg: "screen-y"
            }
        },
        dimensions: {
            width: {
                doc: "Screen width in pixels (auto-detected if not set)",
                format: "nat",
                default: null,
                env: "KONFLIKT_SCREEN_WIDTH",
                arg: "screen-width"
            },
            height: {
                doc: "Screen height in pixels (auto-detected if not set)",
                format: "nat", 
                default: null,
                env: "KONFLIKT_SCREEN_HEIGHT",
                arg: "screen-height"
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
            arg: "screen-edges"
        }
    },

    // Cluster topology - defines relationships between instances
    cluster: {
        topology: {
            doc: "Cluster topology mode",
            format: ["automatic", "manual", "star", "mesh"],
            default: "automatic",
            env: "KONFLIKT_TOPOLOGY",
            arg: "topology"
        },
        
        // Server configuration (for star topology)
        server: {
            host: {
                doc: "Server host to connect to (for client mode)",
                format: String,
                default: null,
                env: "KONFLIKT_SERVER_HOST",
                arg: "server-host"
            },
            port: {
                doc: "Server port to connect to (for client mode)",
                format: "port",
                default: null,
                env: "KONFLIKT_SERVER_PORT", 
                arg: "server-port"
            }
        },

        // Manual peer definitions
        peers: {
            doc: "Manually defined peer connections",
            format: Array,
            default: [],
            env: "KONFLIKT_PEERS",
            arg: "peers"
        },

        // Screen adjacency map (which screens are next to each other)
        adjacency: {
            doc: "Screen adjacency definitions for cursor flow",
            format: Object,
            default: {},
            env: "KONFLIKT_ADJACENCY",
            arg: "adjacency"
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
                arg: "capture-mouse"
            },
            keyboard: {
                doc: "Capture keyboard events",
                format: Boolean,
                default: true,
                env: "KONFLIKT_CAPTURE_KEYBOARD", 
                arg: "capture-keyboard"
            }
        },
        forward: {
            doc: "Which event types to forward to peers",
            format: Array,
            default: ["mouse_move", "mouse_press", "mouse_release", "key_press", "key_release"],
            env: "KONFLIKT_FORWARD_EVENTS",
            arg: "forward-events"
        },
        cursorTransition: {
            enabled: {
                doc: "Enable automatic cursor transition between screens",
                format: Boolean,
                default: true,
                env: "KONFLIKT_CURSOR_TRANSITION",
                arg: "cursor-transition"
            },
            deadZone: {
                doc: "Dead zone in pixels at screen edges before transition",
                format: "nat",
                default: 5,
                env: "KONFLIKT_DEAD_ZONE",
                arg: "dead-zone"
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
            arg: "log-level"
        },
        file: {
            doc: "Log file path",
            format: String,
            default: null,
            env: "KONFLIKT_LOG_FILE",
            arg: "log-file"
        }
    },

    // Development/debugging
    development: {
        enabled: {
            doc: "Enable development mode",
            format: Boolean,
            default: false,
            env: "NODE_ENV",
            arg: "dev"
        },
        mockInput: {
            doc: "Mock input events for testing",
            format: Boolean,
            default: false,
            env: "KONFLIKT_MOCK_INPUT",
            arg: "mock-input"
        }
    }
});

export type ConfigType = ReturnType<typeof configSchema.getProperties>;