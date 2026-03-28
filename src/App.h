#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <memory>
#include <string>

#include "Config.h"

class OptionsWindow;
class TaskbarWidget;

class App {
public:
    App(HINSTANCE instance, std::wstring module_path);
    ~App();

    bool Initialize();
    int Run();

    void ShowOptions();
    void UpdateConfig(const AppConfig& config);
    void RequestExit();

    HINSTANCE GetInstance() const { return instance_; }
    const AppConfig& GetConfig() const { return config_; }

private:
    void SyncAutoStart() const;

    HINSTANCE instance_ = nullptr;
    std::wstring module_path_;
    AppConfig config_{};
    ConfigManager config_manager_;
    std::unique_ptr<TaskbarWidget> taskbar_widget_;
    std::unique_ptr<OptionsWindow> options_window_;
    bool exit_requested_ = false;
};
