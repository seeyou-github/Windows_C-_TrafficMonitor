#pragma once

#include <string>

struct AppConfig {
    int update_interval_ms = 1000;
    bool aggregate_connected_interfaces = true;
    int font_size = 11;
    unsigned int font_color_rgb = 0x009BA6AB;
    int bottom_padding = 1;
    int line_height = 18;
    unsigned int progress_color_rgb = 0x000C1065;
    int horizontal_padding = 3;

    bool operator==(const AppConfig& other) const;
    bool operator!=(const AppConfig& other) const;
};

class ConfigManager {
public:
    explicit ConfigManager(std::wstring module_path);

    bool Load(AppConfig& config) const;
    bool SaveIfChanged(const AppConfig& config);

    const std::wstring& GetConfigPath() const { return config_path_; }

private:
    std::wstring module_path_;
    std::wstring config_path_;

    std::wstring BuildContent(const AppConfig& config) const;
    static std::wstring GetDirectoryFromPath(const std::wstring& path);
};
