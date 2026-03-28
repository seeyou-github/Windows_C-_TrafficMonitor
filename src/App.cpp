#include "App.h"

#include "OptionsWindow.h"
#include "TaskbarWidget.h"

namespace {

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"TaskbarMonitor";

}  // namespace

App::App(HINSTANCE instance, std::wstring module_path)
    : instance_(instance),
      module_path_(std::move(module_path)),
      config_manager_(module_path_) {
}

App::~App() = default;

bool App::Initialize() {
    config_manager_.Load(config_);

    taskbar_widget_ = std::make_unique<TaskbarWidget>(*this);
    options_window_ = std::make_unique<OptionsWindow>(*this);

    if (!taskbar_widget_->Create(instance_)) {
        return false;
    }
    if (!options_window_->Create(instance_)) {
        return false;
    }

    taskbar_widget_->Show();
    SyncAutoStart();
    return true;
}

int App::Run() {
    MSG message{};
    while (!exit_requested_ && ::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void App::ShowOptions() {
    if (options_window_ != nullptr) {
        options_window_->Show();
    }
}

bool App::UpdateConfig(const AppConfig& config) {
    config_ = config;
    const bool config_saved = config_manager_.SaveIfChanged(config_);
    const bool auto_start_synced = SyncAutoStart();
    if (taskbar_widget_ != nullptr) {
        taskbar_widget_->ApplyConfig();
    }
    return config_saved && auto_start_synced;
}

bool App::SyncAutoStart() const {
    HKEY run_key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER,
                          kRunKeyPath,
                          0,
                          nullptr,
                          0,
                          KEY_SET_VALUE,
                          nullptr,
                          &run_key,
                          nullptr) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (config_.auto_start) {
        const std::wstring command = L"\"" + module_path_ + L"\"";
        result = ::RegSetValueExW(run_key,
                                  kRunValueName,
                                  0,
                                  REG_SZ,
                                  reinterpret_cast<const BYTE*>(command.c_str()),
                                  static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = ::RegDeleteValueW(run_key, kRunValueName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }

    ::RegCloseKey(run_key);
    return result == ERROR_SUCCESS;
}

void App::RequestExit() {
    exit_requested_ = true;
    ::PostQuitMessage(0);
}
