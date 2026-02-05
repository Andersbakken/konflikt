#pragma once

#include "Konflikt.h"

#include <optional>
#include <string>
#include <vector>

namespace konflikt {

/// Configuration manager for loading/saving settings
class ConfigManager
{
public:
    /// Get user-specific config file path
    /// macOS: ~/Library/Application Support/Konflikt/config.json
    /// Linux: $XDG_CONFIG_HOME/konflikt/config.json (default: ~/.config/konflikt/)
    static std::string getUserConfigPath();

    /// Get system-wide config file paths (in priority order)
    /// macOS: /Library/Application Support/Konflikt/config.json
    /// Linux: $XDG_CONFIG_DIRS/konflikt/config.json (default: /etc/xdg/konflikt/)
    static std::vector<std::string> getSystemConfigPaths();

    /// Get default config file path (first existing path, or user path for new configs)
    /// Searches: user config, then system config paths
    static std::string getDefaultConfigPath();

    /// Load configuration from file
    /// @param path Path to config file (or empty for default)
    /// @return Config if successful, nullopt if file doesn't exist or is invalid
    static std::optional<Config> load(const std::string &path = "");

    /// Save configuration to file
    /// @param config Configuration to save
    /// @param path Path to config file (or empty for default)
    /// @return true if successful
    static bool save(const Config &config, const std::string &path = "");

    /// Merge command-line options with loaded config
    /// Command-line options take precedence over config file
    static Config merge(const Config &fileConfig, const Config &cmdLineConfig);
};

} // namespace konflikt
