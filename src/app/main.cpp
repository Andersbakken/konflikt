// Konflikt Linux CLI Application

#include <konflikt/ConfigManager.h>
#include <konflikt/Konflikt.h>
#include <konflikt/Version.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

using konflikt::VERSION;

namespace {

konflikt::Konflikt *gKonflikt = nullptr;

void signalHandler(int signal)
{
    if (gKonflikt) {
        std::cout << "\nShutting down..." << std::endl;
        gKonflikt->quit();
    }
    std::exit(signal == SIGINT ? 0 : 1);
}

void printUsage(const char *programName)
{
    std::cout << "Konflikt - Software KVM Switch v" << VERSION << "\n"
              << "\n"
              << "Usage: " << programName << " [OPTIONS]\n"
              << "\n"
              << "Options:\n"
              << "  --role=server|client  Run as server or client (default: server)\n"
              << "  --server=HOST         Server hostname (client auto-discovers if not set)\n"
              << "  --port=PORT           Port to use (default: 3000)\n"
              << "  --config=PATH         Path to config file\n"
              << "  --ui-dir=PATH         Directory containing UI files\n"
              << "  --name=NAME           Display name for this machine\n"
              << "  --no-edge-left        Disable left edge screen transition\n"
              << "  --no-edge-right       Disable right edge screen transition\n"
              << "  --no-edge-top         Disable top edge screen transition\n"
              << "  --no-edge-bottom      Disable bottom edge screen transition\n"
              << "  --lock-cursor         Lock cursor to current screen\n"
              << "  --tls                 Enable TLS/WSS for secure connections\n"
              << "  --tls-cert=PATH       Path to TLS certificate file (PEM)\n"
              << "  --tls-key=PATH        Path to TLS private key file (PEM)\n"
              << "  --tls-passphrase=PASS Passphrase for encrypted key (optional)\n"
              << "  --debug-api           Enable debug API endpoint (/api/log)\n"
              << "  --verbose             Enable verbose logging\n"
              << "  -v, --version         Show version information\n"
              << "  -h, --help            Show this help message\n"
              << std::endl;
}

void printVersion()
{
    std::cout << "Konflikt v" << VERSION << "\n"
              << "Software KVM Switch for Linux and macOS\n"
              << "https://github.com/Andersbakken/konflikt\n"
              << std::endl;
}

std::string getDefaultUiDir()
{
    // Try various locations for the UI files
    std::vector<std::string> candidates = {
        "./dist/ui",
        "../dist/ui",
        "/usr/share/konflikt/ui",
        "/usr/local/share/konflikt/ui",
    };

    for (const auto &path : candidates) {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            return std::filesystem::canonical(path).string();
        }
    }

    return "";
}

} // namespace

int main(int argc, char *argv[])
{
    std::string configPath;
    bool explicitRole = false;

    // First pass: check for --config, --help, --version
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        } else if (arg.rfind("--config=", 0) == 0) {
            configPath = arg.substr(9);
        } else if (arg.rfind("--role=", 0) == 0) {
            explicitRole = true;
        }
    }

    // Load config from file (if exists)
    konflikt::Config config;
    auto fileConfig = konflikt::ConfigManager::load(configPath);
    if (fileConfig) {
        config = *fileConfig;
        std::cout << "Loaded config from " <<
            (configPath.empty() ? konflikt::ConfigManager::getDefaultConfigPath() : configPath) << std::endl;
    }

    // Set defaults for anything not in config
    if (config.instanceName.empty()) {
        config.instanceName = "Linux";
    }
    if (config.uiPath.empty()) {
        config.uiPath = getDefaultUiDir();
    }

    // Parse command line arguments (override config file)
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "-v" || arg == "--version") {
            continue; // Already handled
        } else if (arg.rfind("--config=", 0) == 0) {
            continue; // Already handled
        } else if (arg == "--verbose") {
            config.verbose = true;
        } else if (arg.rfind("--role=", 0) == 0) {
            std::string role = arg.substr(7);
            if (role == "server") {
                config.role = konflikt::InstanceRole::Server;
            } else if (role == "client") {
                config.role = konflikt::InstanceRole::Client;
            } else {
                std::cerr << "Error: Invalid role '" << role << "'. Use 'server' or 'client'." << std::endl;
                return 1;
            }
        } else if (arg.rfind("--server=", 0) == 0) {
            config.serverHost = arg.substr(9);
        } else if (arg.rfind("--port=", 0) == 0) {
            int port = std::stoi(arg.substr(7));
            config.port = port;
            config.serverPort = port;
        } else if (arg.rfind("--ui-dir=", 0) == 0) {
            config.uiPath = arg.substr(9);
        } else if (arg.rfind("--name=", 0) == 0) {
            config.instanceName = arg.substr(7);
        } else if (arg == "--no-edge-left") {
            config.edgeLeft = false;
        } else if (arg == "--no-edge-right") {
            config.edgeRight = false;
        } else if (arg == "--no-edge-top") {
            config.edgeTop = false;
        } else if (arg == "--no-edge-bottom") {
            config.edgeBottom = false;
        } else if (arg == "--lock-cursor") {
            config.lockCursorToScreen = true;
        } else if (arg == "--tls") {
            config.useTLS = true;
        } else if (arg.rfind("--tls-cert=", 0) == 0) {
            config.tlsCertFile = arg.substr(11);
            config.useTLS = true;
        } else if (arg.rfind("--tls-key=", 0) == 0) {
            config.tlsKeyFile = arg.substr(10);
            config.useTLS = true;
        } else if (arg.rfind("--tls-passphrase=", 0) == 0) {
            config.tlsKeyPassphrase = arg.substr(17);
        } else if (arg == "--debug-api") {
            config.enableDebugApi = true;
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Note: Clients without --server will use auto-discovery via mDNS

    // Create Konflikt instance
    konflikt::Konflikt konflikt(config);
    gKonflikt = &konflikt;

    // Set up logging callback
    konflikt.setLogCallback([&config](const std::string &level, const std::string &message) {
        if (level == "verbose" || level == "debug") {
            if (config.verbose) {
                std::cout << "[" << level << "] " << message << std::endl;
            }
        } else if (level == "error") {
            std::cerr << "[ERROR] " << message << std::endl;
        } else {
            std::cout << "[" << level << "] " << message << std::endl;
        }
    });

    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Initialize
    if (!konflikt.init()) {
        std::cerr << "Failed to initialize Konflikt" << std::endl;
        return 1;
    }

    // Print startup info
    std::cout << "Konflikt "
              << (config.role == konflikt::InstanceRole::Server ? "server" : "client")
              << " started" << std::endl;

    if (config.role == konflikt::InstanceRole::Server) {
        std::cout << "Listening on port " << konflikt.httpPort() << std::endl;
        if (!config.uiPath.empty()) {
            std::cout << "UI available at http://localhost:" << konflikt.httpPort() << "/ui/" << std::endl;
        }
    } else {
        if (config.serverHost.empty()) {
            std::cout << "Looking for servers via auto-discovery..." << std::endl;
        } else {
            std::cout << "Connecting to " << config.serverHost << ":" << config.serverPort << std::endl;
        }
    }

    std::cout << "Press Ctrl+C to exit" << std::endl;

    // Run event loop (blocking)
    konflikt.run();

    // Cleanup
    konflikt.stop();
    gKonflikt = nullptr;

    return 0;
}
