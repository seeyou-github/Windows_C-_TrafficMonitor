#include "OptionsWindow.h"

#include "App.h"
#include "UiResources.h"
#include "resource.h"

#include <commdlg.h>
#include <vector>
#include <windowsx.h>

OptionsWindow::OptionsWindow(App& app) : app_(app) {
}

bool OptionsWindow::Create(HINSTANCE instance) {
    instance_ = instance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = L"TaskbarMonitorOptionsWindow";
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

    ::RegisterClassExW(&wc);

    hwnd_ = ::CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        LoadStringResource(instance_, IDS_OPTIONS_TITLE).c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        620,
        420,
        nullptr,
        nullptr,
        instance_,
        this);

    return hwnd_ != nullptr;
}

void OptionsWindow::Show() {
    RefreshFromConfig();
    CenterWindow();
    ::ShowWindow(hwnd_, SW_SHOWNORMAL);
    ::SetForegroundWindow(hwnd_);
}

void OptionsWindow::Hide() {
    if (hwnd_ != nullptr) {
        ::ShowWindow(hwnd_, SW_HIDE);
    }
}

bool OptionsWindow::IsVisible() const {
    return hwnd_ != nullptr && ::IsWindowVisible(hwnd_) != FALSE;
}

void OptionsWindow::RefreshFromConfig() {
    if (hwnd_ == nullptr) {
        return;
    }

    const AppConfig& config = app_.GetConfig();
    ::SetWindowTextW(interval_edit_, std::to_wstring(config.update_interval_ms).c_str());
    ::SetWindowTextW(font_size_edit_, std::to_wstring(config.font_size).c_str());
    ::SetWindowTextW(bottom_padding_edit_, std::to_wstring(config.bottom_padding).c_str());
    ::SetWindowTextW(line_height_edit_, std::to_wstring(config.line_height).c_str());
    ::SetWindowTextW(horizontal_padding_edit_, std::to_wstring(config.horizontal_padding).c_str());
    Button_SetCheck(aggregate_check_,
                    config.aggregate_connected_interfaces ? BST_CHECKED : BST_UNCHECKED);
    selected_font_color_ = static_cast<COLORREF>(config.font_color_rgb);
    selected_bar_color_ = static_cast<COLORREF>(config.progress_color_rgb);
    UpdateColorButtonText();
    UpdateBarColorButtonText();
    UpdateStatusLabel(LoadStringResource(instance_, IDS_OPTIONS_STATUS));
}

LRESULT CALLBACK OptionsWindow::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    OptionsWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = static_cast<OptionsWindow*>(create_struct->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<OptionsWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->HandleMessage(message, w_param, l_param);
    }

    return ::DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT OptionsWindow::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_CREATE:
        font_ = CreateUiFont();
        CreateChildControls();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(w_param)) {
        case IDC_SAVE_BUTTON:
            ApplyConfigFromControls(true);
            return 0;
        case IDC_APPLY_BUTTON:
            ApplyConfigFromControls(false);
            return 0;
        case IDC_FONT_COLOR_BUTTON:
            ChooseFontColor();
            return 0;
        case IDC_BAR_COLOR_BUTTON:
            ChooseBarColor();
            return 0;
        case IDC_CANCEL_BUTTON:
            Hide();
            return 0;
        default:
            break;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(w_param);
        ::SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_BTNFACE));
    }
    case WM_CLOSE:
        Hide();
        return 0;
    case WM_DESTROY:
        if (font_ != nullptr) {
            ::DeleteObject(font_);
            font_ = nullptr;
        }
        return 0;
    default:
        break;
    }

    return ::DefWindowProcW(hwnd_, message, w_param, l_param);
}

void OptionsWindow::CreateChildControls() {
    const int left_margin = 24;
    const int right_margin = 24;
    const int row_top = 24;
    const int edit_top = 50;
    const int column_gap = 12;
    const int total_width = 620 - left_margin - right_margin;
    const int column_width = (total_width - column_gap * 4) / 5;
    const int col1 = left_margin;
    const int col2 = col1 + column_width + column_gap;
    const int col3 = col2 + column_width + column_gap;
    const int col4 = col3 + column_width + column_gap;
    const int col5 = col4 + column_width + column_gap;

    const auto create_label = [&](UINT text_id, int x, int y, int w, int h, int control_id) {
        HWND control = ::CreateWindowExW(
            0,
            L"STATIC",
            LoadStringResource(instance_, text_id).c_str(),
            WS_CHILD | WS_VISIBLE,
            x,
            y,
            w,
            h,
            hwnd_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)),
            instance_,
            nullptr);
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return control;
    };

    create_label(IDS_OPTIONS_INTERVAL, col1, row_top, column_width, 24, -1);

    interval_edit_ = ::CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        col1,
        edit_top,
        column_width,
        28,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INTERVAL_EDIT)),
        instance_,
        nullptr);

    create_label(IDS_OPTIONS_FONT_SIZE, col2, row_top, column_width, 24, -1);

    font_size_edit_ = ::CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        col2,
        edit_top,
        column_width,
        28,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FONT_SIZE_EDIT)),
        instance_,
        nullptr);

    create_label(IDS_OPTIONS_LINE_HEIGHT, col3, row_top, column_width, 24, -1);

    line_height_edit_ = ::CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        col3,
        edit_top,
        column_width,
        28,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LINE_HEIGHT_EDIT)),
        instance_,
        nullptr);

    create_label(IDS_OPTIONS_BOTTOM_PADDING, col4, row_top, column_width, 24, -1);

    bottom_padding_edit_ = ::CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        col4,
        edit_top,
        column_width,
        28,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BOTTOM_PADDING_EDIT)),
        instance_,
        nullptr);

    create_label(IDS_OPTIONS_HORIZONTAL_PADDING, col5, row_top, column_width, 24, -1);

    horizontal_padding_edit_ = ::CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        col5,
        edit_top,
        column_width,
        28,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_HORIZONTAL_PADDING_EDIT)),
        instance_,
        nullptr);

    aggregate_check_ = ::CreateWindowExW(
        0,
        L"BUTTON",
        LoadStringResource(instance_, IDS_OPTIONS_NETMODE).c_str(),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        24,
        96,
        560,
        24,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AGGREGATE_CHECK)),
        instance_,
        nullptr);

    create_label(IDS_OPTIONS_FONT_COLOR, 24, 132, 80, 24, -1);

    font_color_button_ = ::CreateWindowExW(
        0,
        L"BUTTON",
        LoadStringResource(instance_, IDS_OPTIONS_PICK_COLOR).c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        112,
        128,
        150,
        28,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FONT_COLOR_BUTTON)),
        instance_,
        nullptr);

    create_label(IDS_OPTIONS_BAR_COLOR, 290, 132, 90, 24, -1);

    bar_color_button_ = ::CreateWindowExW(
        0,
        L"BUTTON",
        LoadStringResource(instance_, IDS_OPTIONS_PICK_COLOR).c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        386,
        128,
        150,
        28,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BAR_COLOR_BUTTON)),
        instance_,
        nullptr);

    status_static_ = create_label(IDS_OPTIONS_STATUS, 24, 234, 420, 20, IDC_STATUS_STATIC);
    future_static_ = create_label(IDS_OPTIONS_FUTURE, 24, 258, 450, 52, IDC_FUTURE_STATIC);

    apply_button_ = ::CreateWindowExW(
        0,
        L"BUTTON",
        LoadStringResource(instance_, IDS_OPTIONS_APPLY).c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        128,
        352,
        88,
        30,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_APPLY_BUTTON)),
        instance_,
        nullptr);

    save_button_ = ::CreateWindowExW(
        0,
        L"BUTTON",
        LoadStringResource(instance_, IDS_OPTIONS_SAVE).c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        224,
        352,
        88,
        30,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SAVE_BUTTON)),
        instance_,
        nullptr);

    cancel_button_ = ::CreateWindowExW(
        0,
        L"BUTTON",
        LoadStringResource(instance_, IDS_OPTIONS_CANCEL).c_str(),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        320,
        352,
        88,
        30,
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CANCEL_BUTTON)),
        instance_,
        nullptr);

    const HWND controls[] = {interval_edit_, font_size_edit_, line_height_edit_, bottom_padding_edit_,
                             horizontal_padding_edit_,
                             aggregate_check_, status_static_, future_static_, font_color_button_,
                             bar_color_button_, apply_button_, save_button_, cancel_button_};
    for (HWND control : controls) {
        ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

void OptionsWindow::ApplyDarkMode() {
}

void OptionsWindow::CenterWindow() {
    RECT rect{};
    ::GetWindowRect(hwnd_, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;

    RECT work_area{};
    ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);

    const int x = work_area.left + ((work_area.right - work_area.left) - width) / 2;
    const int y = work_area.top + ((work_area.bottom - work_area.top) - height) / 2;
    ::SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void OptionsWindow::UpdateStatusLabel(const std::wstring& text) {
    if (status_static_ != nullptr) {
        ::SetWindowTextW(status_static_, text.c_str());
    }
}

void OptionsWindow::ApplyConfigFromControls(bool close_after_apply) {
    const AppConfig new_config = ReadControls();
    if (new_config.update_interval_ms < 500 || new_config.update_interval_ms > 5000 ||
        new_config.font_size < 8 || new_config.font_size > 18 ||
        new_config.bottom_padding < 0 || new_config.bottom_padding > 24 ||
        new_config.line_height < 14 || new_config.line_height > 32 ||
        new_config.horizontal_padding < 0 || new_config.horizontal_padding > 24) {
        ::MessageBoxW(hwnd_,
                      LoadStringResource(instance_, IDS_INVALID_INTERVAL).c_str(),
                      LoadStringResource(instance_, IDS_APP_NAME).c_str(),
                      MB_OK | MB_ICONWARNING);
        return;
    }

    app_.UpdateConfig(new_config);
    UpdateStatusLabel(LoadStringResource(instance_, IDS_OPTIONS_SAVED));
    if (close_after_apply) {
        Hide();
    }
}

AppConfig OptionsWindow::ReadControls() const {
    AppConfig config = app_.GetConfig();

    wchar_t buffer[64]{};
    ::GetWindowTextW(interval_edit_, buffer, static_cast<int>(std::size(buffer)));
    try {
        config.update_interval_ms = std::stoi(buffer);
    } catch (...) {
        config.update_interval_ms = 0;
    }

    ::GetWindowTextW(font_size_edit_, buffer, static_cast<int>(std::size(buffer)));
    try {
        config.font_size = std::stoi(buffer);
    } catch (...) {
        config.font_size = 0;
    }

    ::GetWindowTextW(bottom_padding_edit_, buffer, static_cast<int>(std::size(buffer)));
    try {
        config.bottom_padding = std::stoi(buffer);
    } catch (...) {
        config.bottom_padding = -1;
    }

    ::GetWindowTextW(line_height_edit_, buffer, static_cast<int>(std::size(buffer)));
    try {
        config.line_height = std::stoi(buffer);
    } catch (...) {
        config.line_height = -1;
    }

    ::GetWindowTextW(horizontal_padding_edit_, buffer, static_cast<int>(std::size(buffer)));
    try {
        config.horizontal_padding = std::stoi(buffer);
    } catch (...) {
        config.horizontal_padding = -1;
    }

    config.aggregate_connected_interfaces =
        (Button_GetCheck(aggregate_check_) == BST_CHECKED);
    config.font_color_rgb = selected_font_color_;
    config.progress_color_rgb = selected_bar_color_;

    return config;
}

HFONT OptionsWindow::CreateUiFont() {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    metrics.lfMessageFont.lfHeight = -16;
    wcscpy_s(metrics.lfMessageFont.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&metrics.lfMessageFont);
}

void OptionsWindow::ChooseFontColor() {
    COLORREF custom_colors[16]{};
    CHOOSECOLORW choose_color{};
    choose_color.lStructSize = sizeof(choose_color);
    choose_color.hwndOwner = hwnd_;
    choose_color.rgbResult = selected_font_color_;
    choose_color.lpCustColors = custom_colors;
    choose_color.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (::ChooseColorW(&choose_color)) {
        selected_font_color_ = choose_color.rgbResult;
        UpdateColorButtonText();
    }
}

void OptionsWindow::UpdateColorButtonText() {
    wchar_t buffer[64]{};
    swprintf_s(buffer,
               L"%s #%02X%02X%02X",
               LoadStringResource(instance_, IDS_OPTIONS_PICK_COLOR).c_str(),
               GetRValue(selected_font_color_),
               GetGValue(selected_font_color_),
               GetBValue(selected_font_color_));
    ::SetWindowTextW(font_color_button_, buffer);
}

void OptionsWindow::ChooseBarColor() {
    COLORREF custom_colors[16]{};
    CHOOSECOLORW choose_color{};
    choose_color.lStructSize = sizeof(choose_color);
    choose_color.hwndOwner = hwnd_;
    choose_color.rgbResult = selected_bar_color_;
    choose_color.lpCustColors = custom_colors;
    choose_color.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (::ChooseColorW(&choose_color)) {
        selected_bar_color_ = choose_color.rgbResult;
        UpdateBarColorButtonText();
    }
}

void OptionsWindow::UpdateBarColorButtonText() {
    wchar_t buffer[64]{};
    swprintf_s(buffer,
               L"%s #%02X%02X%02X",
               LoadStringResource(instance_, IDS_OPTIONS_PICK_COLOR).c_str(),
               GetRValue(selected_bar_color_),
               GetGValue(selected_bar_color_),
               GetBValue(selected_bar_color_));
    ::SetWindowTextW(bar_color_button_, buffer);
}
