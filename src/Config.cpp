#include "Config.h"

#include <fstream>
#include <sstream>

namespace {

std::wstring Trim(const std::wstring& value) {
    const wchar_t* whitespace = L" \t\r\n";
    const size_t start = value.find_first_not_of(whitespace);
    if (start == std::wstring::npos) {
        return L"";
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

bool ParseBool(const std::wstring& value, bool default_value) {
    const std::wstring normalized = Trim(value);
    if (normalized == L"1" || normalized == L"true" || normalized == L"TRUE") {
        return true;
    }
    if (normalized == L"0" || normalized == L"false" || normalized == L"FALSE") {
        return false;
    }
    return default_value;
}

}  // namespace

bool AppConfig::operator==(const AppConfig& other) const {
    return update_interval_ms == other.update_interval_ms &&
           aggregate_connected_interfaces == other.aggregate_connected_interfaces &&
           auto_start == other.auto_start &&
           font_size == other.font_size &&
           font_color_rgb == other.font_color_rgb &&
           bottom_padding == other.bottom_padding &&
           line_height == other.line_height &&
           progress_color_rgb == other.progress_color_rgb &&
           horizontal_padding == other.horizontal_padding;
}

bool AppConfig::operator!=(const AppConfig& other) const {
    return !(*this == other);
}

ConfigManager::ConfigManager(std::wstring module_path)
    : module_path_(std::move(module_path)) {
    config_path_ = GetDirectoryFromPath(module_path_) + L"\\taskbar_monitor.ini";
}

bool ConfigManager::Load(AppConfig& config) const {
    std::wifstream input(config_path_.c_str());
    if (!input.is_open()) {
        return false;
    }

    AppConfig loaded = config;
    std::wstring line;
    while (std::getline(input, line)) {
        const size_t pos = line.find(L'=');
        if (pos == std::wstring::npos) {
            continue;
        }

        const std::wstring key = Trim(line.substr(0, pos));
        const std::wstring value = Trim(line.substr(pos + 1));

        if (key == L"update_interval_ms") {
            try {
                loaded.update_interval_ms = std::stoi(value);
            } catch (...) {
            }
        } else if (key == L"aggregate_connected_interfaces") {
            loaded.aggregate_connected_interfaces =
                ParseBool(value, loaded.aggregate_connected_interfaces);
        } else if (key == L"auto_start") {
            loaded.auto_start = ParseBool(value, loaded.auto_start);
        } else if (key == L"font_size") {
            try {
                loaded.font_size = std::stoi(value);
            } catch (...) {
            }
        } else if (key == L"font_color_rgb") {
            try {
                loaded.font_color_rgb = static_cast<unsigned int>(std::stoul(value));
            } catch (...) {
            }
        } else if (key == L"bottom_padding") {
            try {
                loaded.bottom_padding = std::stoi(value);
            } catch (...) {
            }
        } else if (key == L"line_height") {
            try {
                loaded.line_height = std::stoi(value);
            } catch (...) {
            }
        } else if (key == L"progress_color_rgb") {
            try {
                loaded.progress_color_rgb = static_cast<unsigned int>(std::stoul(value));
            } catch (...) {
            }
        } else if (key == L"horizontal_padding") {
            try {
                loaded.horizontal_padding = std::stoi(value);
            } catch (...) {
            }
        }
    }

    if (loaded.update_interval_ms < 500) {
        loaded.update_interval_ms = 500;
    }
    if (loaded.update_interval_ms > 5000) {
        loaded.update_interval_ms = 5000;
    }
    if (loaded.font_size < 8) {
        loaded.font_size = 8;
    }
    if (loaded.font_size > 18) {
        loaded.font_size = 18;
    }
    if (loaded.bottom_padding < 0) {
        loaded.bottom_padding = 0;
    }
    if (loaded.bottom_padding > 24) {
        loaded.bottom_padding = 24;
    }
    if (loaded.line_height < 14) {
        loaded.line_height = 14;
    }
    if (loaded.line_height > 32) {
        loaded.line_height = 32;
    }
    if (loaded.horizontal_padding < 0) {
        loaded.horizontal_padding = 0;
    }
    if (loaded.horizontal_padding > 24) {
        loaded.horizontal_padding = 24;
    }

    config = loaded;
    return true;
}

bool ConfigManager::SaveIfChanged(const AppConfig& config) {
    const std::wstring new_content = BuildContent(config);

    std::wifstream existing(config_path_.c_str());
    if (existing.is_open()) {
        std::wstringstream buffer;
        buffer << existing.rdbuf();
        if (buffer.str() == new_content) {
            return true;
        }
    }

    std::wofstream output(config_path_.c_str(), std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << new_content;
    return output.good();
}

std::wstring ConfigManager::BuildContent(const AppConfig& config) const {
    std::wstringstream content;
    content << L"update_interval_ms=" << config.update_interval_ms << L"\n";
    content << L"aggregate_connected_interfaces="
            << (config.aggregate_connected_interfaces ? L"true" : L"false") << L"\n";
    content << L"auto_start=" << (config.auto_start ? L"true" : L"false") << L"\n";
    content << L"font_size=" << config.font_size << L"\n";
    content << L"font_color_rgb=" << config.font_color_rgb << L"\n";
    content << L"bottom_padding=" << config.bottom_padding << L"\n";
    content << L"line_height=" << config.line_height << L"\n";
    content << L"progress_color_rgb=" << config.progress_color_rgb << L"\n";
    content << L"horizontal_padding=" << config.horizontal_padding << L"\n";
    return content.str();
}

std::wstring ConfigManager::GetDirectoryFromPath(const std::wstring& path) {
    const size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return L".";
    }
    return path.substr(0, pos);
}
