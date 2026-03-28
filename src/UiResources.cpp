#include "UiResources.h"

std::wstring LoadStringResource(HINSTANCE instance, UINT id) {
    wchar_t buffer[512]{};
    const int length = ::LoadStringW(instance, id, buffer, static_cast<int>(std::size(buffer)));
    if (length <= 0) {
        return L"";
    }
    return std::wstring(buffer, buffer + length);
}
