#include "App.h"
#include "UiResources.h"
#include "resource.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE single_instance_mutex =
        ::CreateMutexW(nullptr, TRUE, L"Global\\TaskbarMonitorSingleInstance");
    if (single_instance_mutex == nullptr || ::GetLastError() == ERROR_ALREADY_EXISTS) {
        ::MessageBoxW(nullptr,
                      LoadStringResource(instance, IDS_APP_ALREADY_RUNNING).c_str(),
                      LoadStringResource(instance, IDS_APP_NAME).c_str(),
                      MB_OK | MB_ICONINFORMATION);
        if (single_instance_mutex != nullptr) {
            ::CloseHandle(single_instance_mutex);
        }
        return 0;
    }

    wchar_t module_path[MAX_PATH]{};
    ::GetModuleFileNameW(nullptr, module_path, MAX_PATH);

    App app(instance, module_path);
    if (!app.Initialize()) {
        ::MessageBoxW(nullptr,
                      LoadStringResource(instance, IDS_STATUS_EMBED_FAILED).c_str(),
                      LoadStringResource(instance, IDS_APP_NAME).c_str(),
                      MB_OK | MB_ICONERROR);
        ::CloseHandle(single_instance_mutex);
        return 1;
    }

    const int exit_code = app.Run();
    ::ReleaseMutex(single_instance_mutex);
    ::CloseHandle(single_instance_mutex);
    return exit_code;
}
