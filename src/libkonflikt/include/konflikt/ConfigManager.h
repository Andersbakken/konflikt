#pragma once

#include "Konflikt.h"

#include <optional>
#include <string>

namespace konflikt {

/// Configuration manager for loading/saving settings
class ConfigManager
{
public:
    /// Get default config file path (~/.config/konflikt/config.json)
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
