#include "konflikt/ConfigManager.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <glaze/json.hpp>

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
    std::string uiPath;
    bool verbose { false };
    std::string logFile;
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
        "uiPath", &T::uiPath,
        "verbose", &T::verbose,
        "logFile", &T::logFile);
};

namespace konflikt {

std::string ConfigManager::getDefaultConfigPath()
{
    std::string configDir;

#ifdef __APPLE__
    // macOS: ~/Library/Application Support/Konflikt/config.json
    const char *home = std::getenv("HOME");
    if (home) {
        configDir = std::string(home) + "/Library/Application Support/Konflikt";
    }
#else
    // Linux: ~/.config/konflikt/config.json
    const char *xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig) {
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
    config.uiPath = jsonConfig.uiPath;
    config.verbose = jsonConfig.verbose;
    config.logFile = jsonConfig.logFile;

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
    jsonConfig.uiPath = config.uiPath;
    jsonConfig.verbose = config.verbose;
    jsonConfig.logFile = config.logFile;

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
