#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <string>

#include "MonitorService.h"

class App;

class TaskbarWidget {
public:
    explicit TaskbarWidget(App& app);
    ~TaskbarWidget();

    bool Create(HINSTANCE instance);
    void Show();
    void Refresh();
    void ApplyConfig();
    void ShowContextMenu(POINT screen_point);
    HWND GetHwnd() const { return hwnd_; }

private:
    struct BarMetrics {
        RECT rect{};
        int percent = 0;
        std::wstring text;
    };

    static constexpr UINT_PTR kRefreshTimerId = 1;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);

    void InitializeWindowForTaskbar();
    void UpdateTaskbarPlacement();
    bool QueryTaskbarInfo(HWND& taskbar, RECT& taskbar_rect, RECT& notify_rect);
    void Paint(HDC dc);
    void DrawSpeedLine(HDC dc, const RECT& rect) const;
    void DrawUsageBar(HDC dc, const BarMetrics& metrics) const;
    std::wstring FormatSpeed(double bytes_per_sec) const;
    std::wstring FormatSpeedUnit(double bytes_per_sec) const;
    void EnsureTimer();
    void UpdateVisualStyleFromTaskbar();
    COLORREF SampleTaskbarColor() const;
    void DrawTextWithShadow(HDC dc, const std::wstring& text, RECT rect, UINT format, COLORREF text_color) const;
    bool IsHorizontalTaskbar() const;
    void RecreateFont();
    void UpdateBrushes();

    App& app_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND taskbar_hwnd_ = nullptr;
    RECT taskbar_rect_{};
    RECT notify_rect_{};
    MonitorService monitor_;
    SystemSnapshot snapshot_{};
    HFONT font_ = nullptr;
    HBRUSH sampled_background_brush_ = nullptr;
    HBRUSH bar_background_brush_ = nullptr;
    HBRUSH bar_fill_brush_ = nullptr;
    COLORREF sampled_background_color_ = RGB(26, 26, 28);
    COLORREF primary_text_color_ = RGB(184, 184, 188);
    COLORREF secondary_text_color_ = RGB(148, 148, 152);
    COLORREF shadow_text_color_ = RGB(0, 0, 0);
    COLORREF bar_background_color_ = RGB(26, 26, 28);
    COLORREF bar_fill_color_ = RGB(210, 52, 52);
    ULONGLONG last_style_sample_tick_ = 0;
    int width_ = 176;
    int height_ = 98;
    bool horizontal_taskbar_ = true;
};
