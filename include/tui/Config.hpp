#ifndef TUI_CONFIG_HPP
#define TUI_CONFIG_HPP

#include <filesystem>
#include <string>
#include <unordered_map>

// Lightweight YAML-like loader for converter options.
// Parses simple "key: value" lines, ignoring comments (#) and blank lines.
class ConverterConfig {
public:
    bool LoadFromFile(const std::filesystem::path& path);

    // Accessors with defaults.
    std::string GetString(const std::string& key, const std::string& fallback) const;
    int GetInt(const std::string& key, int fallback) const;
    bool GetBool(const std::string& key, bool fallback) const;

    // Mutators.
    void SetString(const std::string& key, const std::string& value);
    void SetInt(const std::string& key, int value);
    void SetBool(const std::string& key, bool value);

    // Persist the current values to disk (simple key: value format).
    bool SaveToFile(const std::filesystem::path& path) const;

private:
    std::unordered_map<std::string, std::string> values_;
};

#endif // TUI_CONFIG_HPP
