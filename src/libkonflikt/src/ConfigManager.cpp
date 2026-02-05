#include "konflikt/ConfigManager.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <glaze/json.hpp>
#include <map>

namespace konflikt {

// Glaze metadata for Config serialization
struct ConfigJson
{
    std::string role;
    std::string instanceId;
    std::string instanceName;
    int port { 3000 };
    std::string serverHost;
    int serverPort { 3000 };
    int screenX { 0 };
    int screenY { 0 };
    int screenWidth { 0 };
    int screenHeight { 0 };
    bool edgeLeft { true };
    bool edgeRight { true };
    bool edgeTop { true };
    bool edgeBottom { true };
    bool lockCursorToScreen { false };
    int lockCursorHotkey { 107 };  // Default: Scroll Lock
    std::string uiPath;
    bool useTLS { false };
    std::string tlsCertFile;
    std::string tlsKeyFile;
    std::string tlsKeyPassphrase;
    bool verbose { false };
    std::string logFile;
    bool enableDebugApi { false };
    std::map<std::string, int> keyRemap;  // String keys for JSON compatibility
    bool logKeycodes { false };
};

} // namespace konflikt

template <>
struct glz::meta<konflikt::ConfigJson>
{
    using T = konflikt::ConfigJson;
    static constexpr auto value = object(
        "role", &T::role,
        "instanceId", &T::instanceId,
        "instanceName", &T::instanceName,
        "port", &T::port,
        "serverHost", &T::serverHost,
        "serverPort", &T::serverPort,
        "screenX", &T::screenX,
        "screenY", &T::screenY,
        "screenWidth", &T::screenWidth,
        "screenHeight", &T::screenHeight,
        "edgeLeft", &T::edgeLeft,
        "edgeRight", &T::edgeRight,
        "edgeTop", &T::edgeTop,
        "edgeBottom", &T::edgeBottom,
        "lockCursorToScreen", &T::lockCursorToScreen,
        "lockCursorHotkey", &T::lockCursorHotkey,
        "uiPath", &T::uiPath,
        "useTLS", &T::useTLS,
        "tlsCertFile", &T::tlsCertFile,
        "tlsKeyFile", &T::tlsKeyFile,
        "tlsKeyPassphrase", &T::tlsKeyPassphrase,
        "verbose", &T::verbose,
        "logFile", &T::logFile,
        "enableDebugApi", &T::enableDebugApi,
        "keyRemap", &T::keyRemap,
        "logKeycodes", &T::logKeycodes);
};

namespace konflikt {

std::string ConfigManager::getUserConfigPath()
{
    std::string configDir;

#ifdef __APPLE__
    // macOS: ~/Library/Application Support/Konflikt/config.json
    const char *home = std::getenv("HOME");
    if (home) {
        configDir = std::string(home) + "/Library/Application Support/Konflikt";
    }
#else
    // Linux (XDG spec): $XDG_CONFIG_HOME/konflikt/config.json
    const char *xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && xdgConfig[0] != '\0') {
        configDir = std::string(xdgConfig) + "/konflikt";
    } else {
        const char *home = std::getenv("HOME");
        if (home) {
            configDir = std::string(home) + "/.config/konflikt";
        }
    }
#endif

    if (configDir.empty()) {
        return "";
    }

    return configDir + "/config.json";
}

std::vector<std::string> ConfigManager::getSystemConfigPaths()
{
    std::vector<std::string> paths;

#ifdef __APPLE__
    // macOS: /Library/Application Support/Konflikt/config.json
    paths.push_back("/Library/Application Support/Konflikt/config.json");
#else
    // Linux (XDG spec): $XDG_CONFIG_DIRS/konflikt/config.json
    // XDG_CONFIG_DIRS is colon-separated, default is /etc/xdg
    const char *xdgConfigDirs = std::getenv("XDG_CONFIG_DIRS");
    std::string dirs = (xdgConfigDirs && xdgConfigDirs[0] != '\0') ? xdgConfigDirs : "/etc/xdg";

    size_t start = 0;
    size_t end;
    while ((end = dirs.find(':', start)) != std::string::npos) {
        if (end > start) {
            paths.push_back(dirs.substr(start, end - start) + "/konflikt/config.json");
        }
        start = end + 1;
    }
    if (start < dirs.length()) {
        paths.push_back(dirs.substr(start) + "/konflikt/config.json");
    }
#endif

    return paths;
}

std::string ConfigManager::getDefaultConfigPath()
{
    // First check user config
    std::string userPath = getUserConfigPath();
    if (!userPath.empty() && std::filesystem::exists(userPath)) {
        return userPath;
    }

    // Then check system config paths
    for (const auto &path : getSystemConfigPaths()) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    // Return user path as default for creating new config
    return userPath;
}

std::optional<Config> ConfigManager::load(const std::string &path)
{
    std::string configPath = path.empty() ? getDefaultConfigPath() : path;

    if (configPath.empty() || !std::filesystem::exists(configPath)) {
        return std::nullopt;
    }

    std::ifstream file(configPath);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    ConfigJson jsonConfig;
    auto error = glz::read_json(jsonConfig, content);
    if (error) {
        return std::nullopt;
    }

    Config config;
    config.role = (jsonConfig.role == "server") ? InstanceRole::Server : InstanceRole::Client;
    config.instanceId = jsonConfig.instanceId;
    config.instanceName = jsonConfig.instanceName;
    config.port = jsonConfig.port;
    config.serverHost = jsonConfig.serverHost;
    config.serverPort = jsonConfig.serverPort;
    config.screenX = jsonConfig.screenX;
    config.screenY = jsonConfig.screenY;
    config.screenWidth = jsonConfig.screenWidth;
    config.screenHeight = jsonConfig.screenHeight;
    config.edgeLeft = jsonConfig.edgeLeft;
    config.edgeRight = jsonConfig.edgeRight;
    config.edgeTop = jsonConfig.edgeTop;
    config.edgeBottom = jsonConfig.edgeBottom;
    config.lockCursorToScreen = jsonConfig.lockCursorToScreen;
    config.lockCursorHotkey = static_cast<uint32_t>(jsonConfig.lockCursorHotkey);
    config.uiPath = jsonConfig.uiPath;
    config.useTLS = jsonConfig.useTLS;
    config.tlsCertFile = jsonConfig.tlsCertFile;
    config.tlsKeyFile = jsonConfig.tlsKeyFile;
    config.tlsKeyPassphrase = jsonConfig.tlsKeyPassphrase;
    config.verbose = jsonConfig.verbose;
    config.logFile = jsonConfig.logFile;
    config.enableDebugApi = jsonConfig.enableDebugApi;

    // Convert string keys to uint32_t for keyRemap
    for (const auto &[key, value] : jsonConfig.keyRemap) {
        try {
            uint32_t fromKey = static_cast<uint32_t>(std::stoul(key));
            uint32_t toKey = static_cast<uint32_t>(value);
            config.keyRemap[fromKey] = toKey;
        } catch (...) {
            // Skip invalid entries
        }
    }

    config.logKeycodes = jsonConfig.logKeycodes;

    return config;
}

bool ConfigManager::save(const Config &config, const std::string &path)
{
    std::string configPath = path.empty() ? getDefaultConfigPath() : path;

    if (configPath.empty()) {
        return false;
    }

    // Create directory if it doesn't exist
    std::filesystem::path dirPath = std::filesystem::path(configPath).parent_path();
    if (!dirPath.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dirPath, ec);
        if (ec) {
            return false;
        }
    }

    ConfigJson jsonConfig;
    jsonConfig.role = (config.role == InstanceRole::Server) ? "server" : "client";
    jsonConfig.instanceId = config.instanceId;
    jsonConfig.instanceName = config.instanceName;
    jsonConfig.port = config.port;
    jsonConfig.serverHost = config.serverHost;
    jsonConfig.serverPort = config.serverPort;
    jsonConfig.screenX = config.screenX;
    jsonConfig.screenY = config.screenY;
    jsonConfig.screenWidth = config.screenWidth;
    jsonConfig.screenHeight = config.screenHeight;
    jsonConfig.edgeLeft = config.edgeLeft;
    jsonConfig.edgeRight = config.edgeRight;
    jsonConfig.edgeTop = config.edgeTop;
    jsonConfig.edgeBottom = config.edgeBottom;
    jsonConfig.lockCursorToScreen = config.lockCursorToScreen;
    jsonConfig.lockCursorHotkey = static_cast<int>(config.lockCursorHotkey);
    jsonConfig.uiPath = config.uiPath;
    jsonConfig.useTLS = config.useTLS;
    jsonConfig.tlsCertFile = config.tlsCertFile;
    jsonConfig.tlsKeyFile = config.tlsKeyFile;
    jsonConfig.tlsKeyPassphrase = config.tlsKeyPassphrase;
    jsonConfig.verbose = config.verbose;
    jsonConfig.logFile = config.logFile;
    jsonConfig.enableDebugApi = config.enableDebugApi;

    // Convert uint32_t keys to string for JSON
    for (const auto &[fromKey, toKey] : config.keyRemap) {
        jsonConfig.keyRemap[std::to_string(fromKey)] = static_cast<int>(toKey);
    }

    jsonConfig.logKeycodes = config.logKeycodes;

    auto json = glz::write_json(jsonConfig);
    if (!json) {
        return false;
    }

    // Pretty print with indentation
    std::string prettyJson = glz::prettify_json(*json);

    std::ofstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    file << prettyJson;
    file.close();

    return true;
}

Config ConfigManager::merge(const Config &fileConfig, const Config &cmdLineConfig)
{
    Config merged = fileConfig;

    // Command-line options override file config when explicitly set
    // (We use empty strings and 0 as "not set" indicators)

    if (!cmdLineConfig.instanceId.empty()) {
        merged.instanceId = cmdLineConfig.instanceId;
    }
    if (!cmdLineConfig.instanceName.empty()) {
        merged.instanceName = cmdLineConfig.instanceName;
    }
    if (!cmdLineConfig.serverHost.empty()) {
        merged.serverHost = cmdLineConfig.serverHost;
    }
    if (!cmdLineConfig.uiPath.empty()) {
        merged.uiPath = cmdLineConfig.uiPath;
    }
    if (!cmdLineConfig.logFile.empty()) {
        merged.logFile = cmdLineConfig.logFile;
    }

    // For role, we need a different check since it defaults to Client
    // Only override if explicitly set via command line (caller should indicate this)

    // Numeric values - only override if non-default
    if (cmdLineConfig.port != 3000) {
        merged.port = cmdLineConfig.port;
    }
    if (cmdLineConfig.serverPort != 3000) {
        merged.serverPort = cmdLineConfig.serverPort;
    }
    if (cmdLineConfig.screenX != 0) {
        merged.screenX = cmdLineConfig.screenX;
    }
    if (cmdLineConfig.screenY != 0) {
        merged.screenY = cmdLineConfig.screenY;
    }
    if (cmdLineConfig.screenWidth != 0) {
        merged.screenWidth = cmdLineConfig.screenWidth;
    }
    if (cmdLineConfig.screenHeight != 0) {
        merged.screenHeight = cmdLineConfig.screenHeight;
    }

    // Verbose is tricky - if cmdLine says verbose, use it
    if (cmdLineConfig.verbose) {
        merged.verbose = true;
    }

    return merged;
}

} // namespace konflikt
