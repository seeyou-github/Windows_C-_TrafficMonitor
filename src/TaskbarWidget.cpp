#include "TaskbarWidget.h"

#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "App.h"
#include "UiResources.h"
#include "resource.h"

namespace {

constexpr COLORREF kBackgroundColor = RGB(26, 26, 28);

struct SpeedDisplay {
    double value = 0.0;
    const wchar_t* unit = L"K/s";
};

SpeedDisplay GetSpeedDisplay(double bytes_per_sec) {
    const double value_k = std::max(0.0, bytes_per_sec) / 1024.0;
    if (value_k >= 1024.0) {
        return SpeedDisplay{value_k / 1024.0, L"M/s"};
    }
    if (value_k < 0.1) {
        return SpeedDisplay{0.0, L"K/s"};
    }
    return SpeedDisplay{value_k, L"K/s"};
}

HICON LoadAppIcon(HINSTANCE instance, bool small_icon) {
    return static_cast<HICON>(::LoadImageW(instance,
                                           MAKEINTRESOURCEW(IDI_APP_ICON),
                                           IMAGE_ICON,
                                           small_icon ? ::GetSystemMetrics(SM_CXSMICON)
                                                      : ::GetSystemMetrics(SM_CXICON),
                                           small_icon ? ::GetSystemMetrics(SM_CYSMICON)
                                                      : ::GetSystemMetrics(SM_CYICON),
                                           LR_DEFAULTCOLOR));
}

HFONT CreateTaskbarFont(int font_size) {
    LOGFONTW font{};
    font.lfHeight = -font_size;
    font.lfWeight = FW_NORMAL;
    font.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(font.lfFaceName, L"Microsoft YaHei UI");
    return ::CreateFontIndirectW(&font);
}

RECT MakeRect(int left, int top, int right, int bottom) {
    RECT rect{left, top, right, bottom};
    return rect;
}

}  // namespace

TaskbarWidget::TaskbarWidget(App& app) : app_(app) {
}

TaskbarWidget::~TaskbarWidget() {
    if (host_hwnd_ != nullptr && ::IsWindow(host_hwnd_)) {
        ::DestroyWindow(host_hwnd_);
        host_hwnd_ = nullptr;
    }
    if (hwnd_ != nullptr && ::IsWindow(hwnd_)) {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (font_ != nullptr) {
        ::DeleteObject(font_);
    }
    if (sampled_background_brush_ != nullptr) {
        ::DeleteObject(sampled_background_brush_);
    }
    if (bar_background_brush_ != nullptr) {
        ::DeleteObject(bar_background_brush_);
    }
    if (bar_fill_brush_ != nullptr) {
        ::DeleteObject(bar_fill_brush_);
    }
}

bool TaskbarWidget::Create(HINSTANCE instance) {
    instance_ = instance;
    taskbar_created_message_ = ::RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = L"TaskbarMonitorWidgetWindow";
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadAppIcon(instance_, false);
    wc.hIconSm = LoadAppIcon(instance_, true);

    ::RegisterClassExW(&wc);

    WNDCLASSEXW host_wc{};
    host_wc.cbSize = sizeof(host_wc);
    host_wc.lpfnWndProc = HostWindowProc;
    host_wc.hInstance = instance_;
    host_wc.lpszClassName = L"TaskbarMonitorHostWindow";
    ::RegisterClassExW(&host_wc);

    RecreateFont();
    UpdateBrushes();
    host_hwnd_ = ::CreateWindowExW(
        WS_EX_TOOLWINDOW,
        host_wc.lpszClassName,
        L"TaskbarMonitorHost",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance_,
        this);
    if (host_hwnd_ == nullptr) {
        return false;
    }

    CreateWidgetWindow();
    return hwnd_ != nullptr;
}

void TaskbarWidget::Show() {
    if (hwnd_ == nullptr || !::IsWindow(hwnd_)) {
        CreateWidgetWindow();
    }
    InitializeWindowForTaskbar();
    if (hwnd_ != nullptr && ::IsWindow(hwnd_)) {
        ::ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        Refresh();
    }
}

void TaskbarWidget::Refresh() {
    snapshot_ = monitor_.Sample(app_.GetConfig().aggregate_connected_interfaces);
    EnsureAttachedToTaskbar();
    const ULONGLONG now = ::GetTickCount64();
    if (now - last_style_sample_tick_ >= 2000 || last_style_sample_tick_ == 0) {
        UpdateVisualStyleFromTaskbar();
        last_style_sample_tick_ = now;
    }
    ::InvalidateRect(hwnd_, nullptr, FALSE);
}

void TaskbarWidget::ApplyConfig() {
    RecreateFont();
    last_style_sample_tick_ = 0;
    EnsureTimer();
    Refresh();
}

void TaskbarWidget::ShowContextMenu(POINT screen_point) {
    HMENU menu = ::LoadMenuW(instance_, MAKEINTRESOURCEW(IDR_MAIN_MENU));
    if (menu == nullptr) {
        return;
    }

    HMENU popup = ::GetSubMenu(menu, 0);
    ::SetForegroundWindow(hwnd_);
    ::TrackPopupMenu(popup,
                     TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN,
                     screen_point.x,
                     screen_point.y,
                     0,
                     hwnd_,
                     nullptr);
    ::DestroyMenu(menu);
}

LRESULT CALLBACK TaskbarWidget::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    TaskbarWidget* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = static_cast<TaskbarWidget*>(create_struct->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<TaskbarWidget*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->HandleMessage(message, w_param, l_param);
    }

    return ::DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT CALLBACK TaskbarWidget::HostWindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    TaskbarWidget* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = static_cast<TaskbarWidget*>(create_struct->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<TaskbarWidget*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->HandleHostMessage(hwnd, message, w_param, l_param);
    }

    return ::DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT TaskbarWidget::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_CREATE:
        EnsureTimer();
        return 0;
    case WM_TIMER:
        if (w_param == kRefreshTimerId) {
            Refresh();
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = ::BeginPaint(hwnd_, &ps);
        Paint(dc);
        ::EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_RBUTTONUP: {
        POINT pt{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
        ::ClientToScreen(hwnd_, &pt);
        ShowContextMenu(pt);
        return 0;
    }
    case WM_LBUTTONDBLCLK:
        app_.ShowOptions();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(w_param)) {
        case IDM_OPTIONS:
            app_.ShowOptions();
            return 0;
        case IDM_EXIT:
            app_.RequestExit();
            return 0;
        default:
            break;
        }
        break;
    case WM_DESTROY:
        ::KillTimer(hwnd_, kRefreshTimerId);
        hwnd_ = nullptr;
        return 0;
    default:
        break;
    }

    return ::DefWindowProcW(hwnd_, message, w_param, l_param);
}

LRESULT TaskbarWidget::HandleHostMessage(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    if (taskbar_created_message_ != 0 && message == taskbar_created_message_) {
        if (hwnd_ == nullptr || !::IsWindow(hwnd_)) {
            CreateWidgetWindow();
        }
        Show();
        return 0;
    }

    switch (message) {
    case WM_DESTROY:
        if (host_hwnd_ == hwnd) {
            host_hwnd_ = nullptr;
        }
        return 0;
    default:
        break;
    }

    return ::DefWindowProcW(hwnd, message, w_param, l_param);
}

void TaskbarWidget::CreateWidgetWindow() {
    hwnd_ = ::CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"TaskbarMonitorWidgetWindow",
        LoadStringResource(instance_, IDS_APP_NAME).c_str(),
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width_,
        height_,
        nullptr,
        nullptr,
        instance_,
        this);

    if (hwnd_ != nullptr) {
        HICON big_icon = LoadAppIcon(instance_, false);
        HICON small_icon = LoadAppIcon(instance_, true);
        ::SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
        ::SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
    }
}

void TaskbarWidget::InitializeWindowForTaskbar() {
    if (!QueryTaskbarInfo(taskbar_hwnd_, taskbar_rect_, notify_rect_)) {
        return;
    }

    LONG_PTR style = ::GetWindowLongPtrW(hwnd_, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_POPUP);
    style |= WS_CHILD | WS_VISIBLE;
    ::SetWindowLongPtrW(hwnd_, GWL_STYLE, style);

    ::SetParent(hwnd_, taskbar_hwnd_);
    ::SetWindowPos(hwnd_, HWND_TOP, 0, 0, width_, height_, SWP_FRAMECHANGED | SWP_NOACTIVATE);
    UpdateTaskbarPlacement();
}

bool TaskbarWidget::EnsureAttachedToTaskbar() {
    HWND current_taskbar = nullptr;
    RECT current_taskbar_rect{};
    RECT current_notify_rect{};
    if (!QueryTaskbarInfo(current_taskbar, current_taskbar_rect, current_notify_rect)) {
        return false;
    }

    const HWND parent = ::GetParent(hwnd_);
    if (current_taskbar != taskbar_hwnd_ || parent != current_taskbar) {
        taskbar_hwnd_ = current_taskbar;
        taskbar_rect_ = current_taskbar_rect;
        notify_rect_ = current_notify_rect;
        InitializeWindowForTaskbar();
        return true;
    }

    UpdateTaskbarPlacement();
    return true;
}

void TaskbarWidget::UpdateTaskbarPlacement() {
    if (!QueryTaskbarInfo(taskbar_hwnd_, taskbar_rect_, notify_rect_)) {
        return;
    }

    const int taskbar_width = taskbar_rect_.right - taskbar_rect_.left;
    const int taskbar_height = taskbar_rect_.bottom - taskbar_rect_.top;
    horizontal_taskbar_ = (taskbar_width >= taskbar_height);

    if (horizontal_taskbar_) {
        width_ = 176;
        height_ = std::max(34, std::min(taskbar_height - 4, app_.GetConfig().line_height * 2 + 6));
    } else {
        width_ = std::max(40, taskbar_width - 4);
        height_ = app_.GetConfig().line_height * 4 + app_.GetConfig().bottom_padding + 14;
    }

    int x = 0;
    int y = 0;

    if (horizontal_taskbar_) {
        const int local_notify_left = notify_rect_.left - taskbar_rect_.left;
        const int local_notify_top = notify_rect_.top - taskbar_rect_.top;
        const int local_notify_height = notify_rect_.bottom - notify_rect_.top;

        x = std::max(4, local_notify_left - width_ - 6);
        y = std::max(2, local_notify_top + std::max(0, (local_notify_height - height_) / 2));
    } else {
        const int local_notify_top = notify_rect_.top - taskbar_rect_.top;
        x = 2;
        y = std::max(2, local_notify_top - height_ - 6);
    }

    ::SetWindowPos(hwnd_, HWND_TOP, x, y, width_, height_, SWP_NOACTIVATE);
}

bool TaskbarWidget::QueryTaskbarInfo(HWND& taskbar, RECT& taskbar_rect, RECT& notify_rect) {
    taskbar = ::FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar == nullptr) {
        return false;
    }

    ::GetWindowRect(taskbar, &taskbar_rect);

    HWND notify = ::FindWindowExW(taskbar, nullptr, L"TrayNotifyWnd", nullptr);
    if (notify != nullptr) {
        ::GetWindowRect(notify, &notify_rect);
    } else {
        notify_rect = taskbar_rect;
    }
    return true;
}

void TaskbarWidget::Paint(HDC dc) {
    RECT client{};
    ::GetClientRect(hwnd_, &client);
    const int bottom_padding = app_.GetConfig().bottom_padding;
    const int horizontal_padding = app_.GetConfig().horizontal_padding;
    const int content_bottom = std::max(client.top, client.bottom - bottom_padding);

    ::FillRect(dc, &client, sampled_background_brush_);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, primary_text_color_);
    ::SelectObject(dc, font_);

    if (horizontal_taskbar_) {
        const int row_height = std::max(14, static_cast<int>((content_bottom - client.top) / 2));
        const int column_gap = 8;
        const int split_x = (client.right - column_gap) / 2;
        const int right_limit = std::max(horizontal_padding, static_cast<int>(client.right - horizontal_padding));

        DrawSpeedLine(dc, MakeRect(horizontal_padding, 0, std::max(horizontal_padding, split_x - horizontal_padding), row_height));
        DrawUsageBar(dc, BarMetrics{MakeRect(split_x + column_gap + horizontal_padding, 0, std::max(split_x + column_gap + horizontal_padding, right_limit), row_height),
                                    snapshot_.cpu_usage_percent,
                                    L"CPU"});

        DrawSpeedLine(dc, MakeRect(horizontal_padding, row_height, std::max(horizontal_padding, split_x - horizontal_padding), content_bottom));
        DrawUsageBar(dc, BarMetrics{MakeRect(split_x + column_gap + horizontal_padding, row_height, std::max(split_x + column_gap + horizontal_padding, right_limit), content_bottom),
                                    snapshot_.memory_usage_percent,
                                    L"\u5185\u5b58"});
    } else {
        const int top0 = 4;
        const int available_height = std::max(4, content_bottom - top0);
        const int row_height = std::max(14, available_height / 4);
        const int right_limit = std::max(horizontal_padding, static_cast<int>(client.right - horizontal_padding));
        const int y1 = top0;
        const int y2 = y1 + row_height;
        const int y3 = y2 + row_height;
        const int y4 = y3 + row_height;
        const int y5 = content_bottom;

        DrawSpeedLine(dc, MakeRect(horizontal_padding, y1, right_limit, y2));
        DrawSpeedLine(dc, MakeRect(horizontal_padding, y2, right_limit, y3));
        DrawUsageBar(dc, BarMetrics{MakeRect(horizontal_padding, y3, right_limit, y4),
                                    snapshot_.cpu_usage_percent,
                                    L"CPU"});
        DrawUsageBar(dc, BarMetrics{MakeRect(horizontal_padding, y4, right_limit, y5),
                                    snapshot_.memory_usage_percent,
                                    L"\u5185\u5b58"});
    }
}

void TaskbarWidget::DrawSpeedLine(HDC dc, const RECT& rect) const {
    ::FillRect(dc, &rect, sampled_background_brush_);

    const bool is_upload_line = (rect.top < 12);
    const double speed = is_upload_line ? snapshot_.upload_bytes_per_sec : snapshot_.download_bytes_per_sec;
    const std::wstring value_text =
        (is_upload_line ? L"\x2191 " : L"\x2193 ") + FormatSpeed(speed);
    const std::wstring unit_text = FormatSpeedUnit(speed);

    RECT unit_rect = rect;
    unit_rect.right -= 2;
    DrawTextWithShadow(dc, unit_text, unit_rect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, secondary_text_color_);

    RECT value_rect = rect;
    value_rect.right -= 26;
    DrawTextWithShadow(dc, value_text, value_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, primary_text_color_);
}

void TaskbarWidget::DrawUsageBar(HDC dc, const BarMetrics& metrics) const {
    RECT bar = metrics.rect;
    ::FillRect(dc, &bar, bar_background_brush_);

    RECT fill = bar;
    fill.top += 1;
    fill.bottom -= 1;
    const int inner_width = std::max(0, static_cast<int>((bar.right - bar.left) - 6));
    fill.left += 0;
    fill.right = fill.left + (inner_width * std::clamp(metrics.percent, 0, 100)) / 100;
    if (fill.right > fill.left) {
        ::FillRect(dc, &fill, bar_fill_brush_);
    }

    RECT text_rect = bar;
    text_rect.left += 0;
    DrawTextWithShadow(dc, metrics.text, text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, primary_text_color_);

    RECT percent_rect = bar;
    percent_rect.right -= 2;
    std::wstring percent_text = std::to_wstring(metrics.percent) + L"%";
    DrawTextWithShadow(dc, percent_text, percent_rect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, secondary_text_color_);
}

std::wstring TaskbarWidget::FormatSpeed(double bytes_per_sec) const {
    const SpeedDisplay display = GetSpeedDisplay(bytes_per_sec);

    wchar_t buffer[64]{};
    if (display.value >= 10.0) {
        swprintf_s(buffer, L"%.0f", std::round(display.value));
    } else {
        swprintf_s(buffer, L"%.1f", display.value);
    }
    return buffer;
}

std::wstring TaskbarWidget::FormatSpeedUnit(double bytes_per_sec) const {
    return GetSpeedDisplay(bytes_per_sec).unit;
}

void TaskbarWidget::EnsureTimer() {
    ::KillTimer(hwnd_, kRefreshTimerId);
    ::SetTimer(hwnd_, kRefreshTimerId, app_.GetConfig().update_interval_ms, nullptr);
}

void TaskbarWidget::UpdateVisualStyleFromTaskbar() {
    const COLORREF sampled = SampleTaskbarColor();
    sampled_background_color_ = sampled;
    const COLORREF configured = static_cast<COLORREF>(app_.GetConfig().font_color_rgb);
    const int r = GetRValue(sampled);
    const int g = GetGValue(sampled);
    const int b = GetBValue(sampled);
    const double luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;

    if (luminance < 118.0) {
        primary_text_color_ = configured;
        secondary_text_color_ =
            RGB(GetRValue(configured) * 4 / 5, GetGValue(configured) * 4 / 5, GetBValue(configured) * 4 / 5);
        shadow_text_color_ = RGB(0, 0, 0);
        bar_background_color_ = sampled_background_color_;
        bar_fill_color_ = static_cast<COLORREF>(app_.GetConfig().progress_color_rgb);
    } else {
        primary_text_color_ = configured;
        secondary_text_color_ =
            RGB(GetRValue(configured) * 4 / 5, GetGValue(configured) * 4 / 5, GetBValue(configured) * 4 / 5);
        shadow_text_color_ = RGB(255, 255, 255);
        bar_background_color_ = sampled_background_color_;
        bar_fill_color_ = static_cast<COLORREF>(app_.GetConfig().progress_color_rgb);
    }
    UpdateBrushes();
}

COLORREF TaskbarWidget::SampleTaskbarColor() const {
    if (taskbar_hwnd_ == nullptr) {
        return kBackgroundColor;
    }

    HDC taskbar_dc = ::GetWindowDC(taskbar_hwnd_);
    if (taskbar_dc == nullptr) {
        return kBackgroundColor;
    }

    RECT window_rect{};
    ::GetWindowRect(hwnd_, &window_rect);
    const int widget_left = static_cast<int>(window_rect.left - taskbar_rect_.left);
    const int widget_top = static_cast<int>(window_rect.top - taskbar_rect_.top);
    const int widget_right = widget_left + width_;
    const int widget_bottom = widget_top + height_;

    int points[9][2]{};
    if (horizontal_taskbar_) {
        const int sample_x_left = std::max(0, widget_left - 10);
        const int sample_x_right =
            std::min(static_cast<int>(taskbar_rect_.right - taskbar_rect_.left - 1), widget_right + 10);
        const int y1 = std::max(0, widget_top + 2);
        const int y2 = std::max(0, widget_top + height_ / 2);
        const int y3 = std::max(0, widget_bottom - 3);
        int temp[9][2] = {
            {sample_x_left, y1}, {sample_x_left, y2}, {sample_x_left, y3},
            {sample_x_right, y1}, {sample_x_right, y2}, {sample_x_right, y3},
            {std::max(0, widget_left - 6), y2}, {std::min(sample_x_right, widget_right + 6), y2}, {sample_x_left, y2}
        };
        memcpy(points, temp, sizeof(points));
    } else {
        const int x1 = std::max(0, widget_left + 2);
        const int x2 = std::max(0, widget_left + width_ / 2);
        const int x3 = std::max(0, widget_right - 3);
        const int sample_y_top = std::max(0, widget_top - 10);
        const int sample_y_bottom =
            std::min(static_cast<int>(taskbar_rect_.bottom - taskbar_rect_.top - 1), widget_bottom + 10);
        int temp[9][2] = {
            {x1, sample_y_top}, {x2, sample_y_top}, {x3, sample_y_top},
            {x1, sample_y_bottom}, {x2, sample_y_bottom}, {x3, sample_y_bottom},
            {x2, std::max(0, widget_top - 6)}, {x2, std::min(sample_y_bottom, widget_bottom + 6)}, {x2, sample_y_top}
        };
        memcpy(points, temp, sizeof(points));
    }

    int total_r = 0;
    int total_g = 0;
    int total_b = 0;
    int valid_count = 0;
    for (const auto& point : points) {
        const int sample_x = std::max(0, point[0]);
        const int sample_y = std::max(0, point[1]);
        const COLORREF color = ::GetPixel(taskbar_dc, sample_x, sample_y);
        if (color == CLR_INVALID) {
            continue;
        }
        total_r += GetRValue(color);
        total_g += GetGValue(color);
        total_b += GetBValue(color);
        ++valid_count;
    }
    ::ReleaseDC(taskbar_hwnd_, taskbar_dc);

    if (valid_count == 0) {
        return kBackgroundColor;
    }

    return RGB(total_r / valid_count, total_g / valid_count, total_b / valid_count);
}

void TaskbarWidget::DrawTextWithShadow(HDC dc,
                                       const std::wstring& text,
                                       RECT rect,
                                       UINT format,
                                       COLORREF text_color) const {
    RECT shadow_rect = rect;
    shadow_rect.left += 1;
    shadow_rect.top += 1;
    shadow_rect.right += 1;
    shadow_rect.bottom += 1;
    ::SetTextColor(dc, shadow_text_color_);
    ::DrawTextW(dc, text.c_str(), -1, &shadow_rect, format);
    ::SetTextColor(dc, text_color);
    ::DrawTextW(dc, text.c_str(), -1, &rect, format);
}

bool TaskbarWidget::IsHorizontalTaskbar() const {
    return horizontal_taskbar_;
}

void TaskbarWidget::RecreateFont() {
    if (font_ != nullptr) {
        ::DeleteObject(font_);
        font_ = nullptr;
    }
    font_ = CreateTaskbarFont(app_.GetConfig().font_size + 5);
}

void TaskbarWidget::UpdateBrushes() {
    if (sampled_background_brush_ != nullptr) {
        ::DeleteObject(sampled_background_brush_);
    }
    if (bar_background_brush_ != nullptr) {
        ::DeleteObject(bar_background_brush_);
    }
    if (bar_fill_brush_ != nullptr) {
        ::DeleteObject(bar_fill_brush_);
    }

    sampled_background_brush_ = ::CreateSolidBrush(sampled_background_color_);
    bar_background_brush_ = ::CreateSolidBrush(bar_background_color_);
    bar_fill_brush_ = ::CreateSolidBrush(bar_fill_color_);
}
