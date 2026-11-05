#include "gui/native_theme.h"
#include <uxtheme.h>
#include <windows.h>
namespace audiohole::gui {
namespace {
using AllowWindow = bool(WINAPI*)(HWND, bool);
using PreferredMode = int(WINAPI*)(int);
struct ThemeApi {
    HMODULE module = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    AllowWindow allow = nullptr;
    PreferredMode preferred = nullptr;
    ThemeApi() {
        // Ordinal 135 changed ABI before Windows 10 build 18362. Never call it
        // on earlier builds. These optional APIs affect this process only.
        OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
        using Version = LONG(WINAPI*)(OSVERSIONINFOW*);
        const auto rtl = reinterpret_cast<Version>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
        if (module && rtl && rtl(&version) == 0 && version.dwBuildNumber >= 18362) {
            allow = reinterpret_cast<AllowWindow>(GetProcAddress(module, MAKEINTRESOURCEA(133)));
            preferred =
                reinterpret_cast<PreferredMode>(GetProcAddress(module, MAKEINTRESOURCEA(135)));
        }
    }
    ~ThemeApi() {
        if (module)
            FreeLibrary(module);
    }
};
ThemeApi& api() {
    static ThemeApi value;
    return value;
}
void apply(HWND handle, bool dark, bool contrast) {
    if (api().allow)
        api().allow(handle, dark && !contrast);
    wchar_t name[64]{};
    GetClassNameW(handle, name, 64);
    const bool combo = wcscmp(name, L"ComboBox") == 0;
    SetWindowTheme(handle,
                   contrast ? nullptr
                   : dark   ? (combo ? L"DarkMode_CFD" : L"DarkMode_Explorer")
                            : L"Explorer",
                   nullptr);
    if (combo) {
        COMBOBOXINFO info{};
        info.cbSize = sizeof(info);
        if (GetComboBoxInfo(handle, &info) && info.hwndList && info.hwndList != handle) {
            if (api().allow)
                api().allow(info.hwndList, dark && !contrast);
            SetWindowTheme(info.hwndList,
                           contrast ? nullptr
                           : dark   ? L"DarkMode_Explorer"
                                    : L"Explorer",
                           nullptr);
        }
    }
    SendMessageW(handle, WM_THEMECHANGED, 0, 0);
    RedrawWindow(handle, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
}
} // namespace
void prepare_native_theme(bool dark, bool contrast) {
    if (api().preferred)
        api().preferred(contrast ? 0 : dark ? 2 : 3);
}
void native_theme(wxWindow* window, bool dark, bool contrast) {
    const auto handle = static_cast<HWND>(window->GetHandle());
    if (!handle)
        return;
    apply(handle, dark, contrast);
    // Native edit buddies, dropdown arrows and scrollbar children aren't all
    // represented in wx's child collection.
    struct Context {
        bool dark, contrast;
    } context{dark, contrast};
    EnumChildWindows(
        handle,
        [](HWND child, LPARAM value) -> BOOL {
            const auto& context = *reinterpret_cast<Context*>(value);
            apply(child, context.dark, context.contrast);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&context));
}
} // namespace audiohole::gui
