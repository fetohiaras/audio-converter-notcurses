#include "tui/Config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace {
std::string Trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])) != 0) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }
    return s.substr(start, end - start);
}
}

bool ConverterConfig::LoadFromFile(const std::filesystem::path& path) {
    values_.clear();
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::size_t colon = trimmed.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string key = Trim(trimmed.substr(0, colon));
        std::string value = Trim(trimmed.substr(colon + 1));
        values_[key] = value;
    }

    return true;
}

std::string ConverterConfig::GetString(const std::string& key, const std::string& fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return fallback;
    }
    return it->second;
}

int ConverterConfig::GetInt(const std::string& key, int fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return fallback;
    }
    try {
        return std::stoi(it->second);
    } catch (...) {
        return fallback;
    }
}

bool ConverterConfig::GetBool(const std::string& key, bool fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        return fallback;
    }
    std::string v = it->second;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (v == "true" || v == "yes" || v == "1" || v == "on") {
        return true;
    }
    if (v == "false" || v == "no" || v == "0" || v == "off") {
        return false;
    }
    return fallback;
}

void ConverterConfig::SetString(const std::string& key, const std::string& value) {
    values_[key] = value;
}

void ConverterConfig::SetInt(const std::string& key, int value) {
    values_[key] = std::to_string(value);
}

void ConverterConfig::SetBool(const std::string& key, bool value) {
    values_[key] = value ? "true" : "false";
}

bool ConverterConfig::SaveToFile(const std::filesystem::path& path) const {
    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }
    for (const auto& kv : values_) {
        out << kv.first << ": " << kv.second << "\n";
    }
    return true;
}
