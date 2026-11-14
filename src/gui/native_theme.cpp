#include "gui/native_theme.h"
#include <algorithm>
#include <commctrl.h>
#include <string>
#include <uxtheme.h>
#include <windows.h>
namespace audiohole::gui {
namespace {
constexpr UINT_PTR border_subclass = 0x41484244;
// Paint integral physical-pixel outlines after native paint. Only application
// input/list/group frames participate; high contrast keeps Windows' own chrome.
LRESULT CALLBACK border_proc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id,
                             DWORD_PTR policy) {
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(handle, border_proc, id);
        return DefSubclassProc(handle, message, wparam, lparam);
    }
    // Native resize blits otherwise copy our old frame into the new client,
    // leaving horizontal trails when list/edit controls grow during sash drags.
    if (message == WM_WINDOWPOSCHANGING && !(policy & 16)) {
        auto* position = reinterpret_cast<WINDOWPOS*>(lparam);
        if (!(position->flags & SWP_NOSIZE))
            position->flags |= SWP_NOCOPYBITS;
    }
    if (message == WM_SIZE && !(policy & 16))
        RedrawWindow(handle, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
    if (message == WM_PAINT && (policy & 8) && !(policy & 2)) {
        bool enabled = true;
        for (HWND current = handle; current; current = GetParent(current))
            enabled = enabled && IsWindowEnabled(current);
        if (!enabled) {
            PAINTSTRUCT paint{};
            const auto dc = BeginPaint(handle, &paint);
            const auto brush = reinterpret_cast<HBRUSH>(
                SendMessageW(GetParent(handle), WM_CTLCOLORSTATIC, reinterpret_cast<WPARAM>(dc),
                             reinterpret_cast<LPARAM>(handle)));
            RECT bounds{};
            GetClientRect(handle, &bounds);
            FillRect(dc, &bounds, brush ? brush : GetSysColorBrush(COLOR_BTNFACE));
            const int saved = SaveDC(dc);
            SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(handle, WM_GETFONT, 0, 0)));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, (policy & 1) ? RGB(150, 160, 175) : RGB(110, 110, 110));
            const int length = GetWindowTextLengthW(handle);
            std::wstring caption(length + 1, L'\0');
            GetWindowTextW(handle, caption.data(), length + 1);
            const auto style = GetWindowLongPtrW(handle, GWL_STYLE) & SS_TYPEMASK;
            DrawTextW(dc, caption.c_str(), length, &bounds,
                      DT_NOPREFIX | DT_WORDBREAK |
                          (style == SS_CENTER  ? DT_CENTER
                           : style == SS_RIGHT ? DT_RIGHT
                                               : DT_LEFT));
            RestoreDC(dc, saved);
            EndPaint(handle, &paint);
            return 0;
        }
    }
    const auto result = DefSubclassProc(handle, message, wparam, lparam);
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS)
        RedrawWindow(handle, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
    if ((message != WM_PAINT && message != WM_NCPAINT) || (policy & (2 | 8)))
        return result;
    RECT bounds{};
    GetWindowRect(handle, &bounds);
    OffsetRect(&bounds, -bounds.left, -bounds.top);
    const auto dc = GetWindowDC(handle);
    if (!dc)
        return result;
    const int saved = SaveDC(dc);
    // GetDpiForWindow is optional on early Windows 10 versions. Do not add an
    // unconditional loader dependency merely to draw application outlines.
    using WindowDpi = UINT(WINAPI*)(HWND);
    static const auto window_dpi = reinterpret_cast<WindowDpi>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    const int dpi =
        window_dpi ? static_cast<int>(window_dpi(handle)) : GetDeviceCaps(dc, LOGPIXELSX);
    const int thickness = std::max(2, MulDiv(2, dpi ? dpi : 96, 96));
    if (policy & 4) {
        // A native group box's top border passes through its caption's baseline.
        const auto font = reinterpret_cast<HFONT>(SendMessageW(handle, WM_GETFONT, 0, 0));
        if (font)
            SelectObject(dc, font);
        TEXTMETRICW metric{};
        GetTextMetricsW(dc, &metric);
        wchar_t caption[256]{};
        GetWindowTextW(handle, caption, 256);
        SIZE text{};
        GetTextExtentPoint32W(dc, caption, lstrlenW(caption), &text);
        bounds.top += metric.tmHeight / 2;
        const int inset = MulDiv(8, dpi ? dpi : 96, 96);
        ExcludeClipRect(dc, inset / 2, 0, inset + text.cx, metric.tmHeight);
    }
    const bool dark = policy & 1;
    const auto colour = GetFocus() == handle ? RGB(40, 170, 145)
                        : dark               ? RGB(96, 120, 132)
                                             : RGB(110, 127, 140);
    const auto brush = CreateSolidBrush(colour);
    for (int pixel = 0; pixel < thickness; ++pixel) {
        FrameRect(dc, &bounds, brush);
        InflateRect(&bounds, -1, -1);
    }
    DeleteObject(brush);
    RestoreDC(dc, saved);
    ReleaseDC(handle, dc);
    return result;
}
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
    const bool group = wcscmp(name, L"Button") == 0 &&
                       (GetWindowLongPtrW(handle, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX;
    const auto style = GetWindowLongPtrW(handle, GWL_STYLE);
    const bool framed_child =
        (style & WS_CHILD) &&
        ((style & WS_BORDER) || (GetWindowLongPtrW(handle, GWL_EXSTYLE) & WS_EX_CLIENTEDGE));
    const bool label = wcscmp(name, L"Static") == 0;
    if (combo || group || framed_child || label)
        SetWindowSubclass(handle, border_proc, border_subclass,
                          (dark ? 1 : 0) | (contrast ? 2 : 0) | (group ? 4 : 0) | (label ? 8 : 0) |
                              (combo ? 16 : 0));
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
            SetWindowSubclass(info.hwndList, border_proc, border_subclass,
                              (dark ? 1 : 0) | (contrast ? 2 : 0));
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
