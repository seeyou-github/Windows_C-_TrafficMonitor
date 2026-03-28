#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <string>

#include "Config.h"

class App;

class OptionsWindow {
public:
    explicit OptionsWindow(App& app);

    bool Create(HINSTANCE instance);
    void Show();
    void Hide();
    bool IsVisible() const;
    HWND GetHwnd() const { return hwnd_; }
    void RefreshFromConfig();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);

    void CreateChildControls();
    void ApplyDarkMode();
    void CenterWindow();
    void UpdateStatusLabel(const std::wstring& text);
    AppConfig ReadControls() const;
    static HFONT CreateUiFont();
    void ChooseFontColor();
    void UpdateColorButtonText();
    void ChooseBarColor();
    void UpdateBarColorButtonText();
    void ApplyConfigFromControls(bool close_after_apply);

    App& app_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND interval_edit_ = nullptr;
    HWND font_size_edit_ = nullptr;
    HWND bottom_padding_edit_ = nullptr;
    HWND line_height_edit_ = nullptr;
    HWND horizontal_padding_edit_ = nullptr;
    HWND aggregate_check_ = nullptr;
    HWND status_static_ = nullptr;
    HWND future_static_ = nullptr;
    HWND font_color_button_ = nullptr;
    HWND bar_color_button_ = nullptr;
    HWND apply_button_ = nullptr;
    HWND save_button_ = nullptr;
    HWND cancel_button_ = nullptr;
    HFONT font_ = nullptr;
    COLORREF selected_font_color_ = RGB(171, 166, 155);
    COLORREF selected_bar_color_ = RGB(101, 16, 12);
};
