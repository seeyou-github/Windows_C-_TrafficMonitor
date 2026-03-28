#include "App.h"

#include "OptionsWindow.h"
#include "TaskbarWidget.h"

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

void App::UpdateConfig(const AppConfig& config) {
    config_ = config;
    config_manager_.SaveIfChanged(config_);
    if (taskbar_widget_ != nullptr) {
        taskbar_widget_->ApplyConfig();
    }
}

void App::RequestExit() {
    exit_requested_ = true;
    ::PostQuitMessage(0);
}
