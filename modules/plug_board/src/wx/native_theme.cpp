#include "native_theme.hpp"
#include "controls.hpp"

#include <windows.h>

#include <commctrl.h>
#include <uxtheme.h>

#include <algorithm>
#include <bit>
#include <new>
#include <oleacc.h>

namespace audiohole::plug_board::desktop {
namespace {
constexpr UINT_PTR theme_subclass = 0x50425448;
constexpr UINT_PTR tracking_timer = 0x50425343;
COLORREF rgb(const wxColour& value) {
    return RGB(value.Red(), value.Green(), value.Blue());
}
struct Palette {
    bool dark{}, contrast{};
    COLORREF surface{}, input{}, foreground{}, muted{}, accent{};
    COLORREF selected_background{}, selected_foreground{};
    bool operator==(const Palette&) const = default;
};
Palette palette(const ViewAppearance& value) {
    const auto selected = item_colors(value, true);
    return {value.dark,        value.high_contrast,      rgb(value.surface),
            rgb(value.input),  rgb(value.foreground),    rgb(value.muted),
            rgb(value.accent), rgb(selected.background), rgb(selected.foreground)};
}
using AllowWindow = bool(WINAPI*)(HWND, bool);
using PreferredMode = int(WINAPI*)(int);
using FlushMenus = void(WINAPI*)();
struct ThemeApi {
    HMODULE library = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    AllowWindow allow{};
    PreferredMode preferred{};
    FlushMenus flush{};
    int previous_mode = -1;
    ThemeApi() {
        // Reuse AudioHole's build-guarded optional Windows theme capability.
        // Ordinal 135 had a different ABI before build 18362. No global OS
        // setting is modified, and custom colors still work if APIs are absent.
        OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
        using Version = LONG(WINAPI*)(OSVERSIONINFOW*);
        const auto query =
            std::bit_cast<Version>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
        if (library && query && query(&version) == 0 && version.dwBuildNumber >= 18362) {
            allow = std::bit_cast<AllowWindow>(GetProcAddress(library, MAKEINTRESOURCEA(133)));
            preferred =
                std::bit_cast<PreferredMode>(GetProcAddress(library, MAKEINTRESOURCEA(135)));
            flush = std::bit_cast<FlushMenus>(GetProcAddress(library, MAKEINTRESOURCEA(136)));
        }
    }
    ~ThemeApi() {
        if (library)
            FreeLibrary(library);
    }
};
ThemeApi& api() {
    static ThemeApi value;
    return value;
}
void fill(HDC dc, const RECT& bounds, COLORREF color) {
    const auto brush = CreateSolidBrush(color);
    FillRect(dc, &bounds, brush);
    DeleteObject(brush);
}
void triangle(HDC dc, const RECT& bounds, bool vertical, bool forward, COLORREF color) {
    const int x = (bounds.left + bounds.right) / 2, y = (bounds.top + bounds.bottom) / 2;
    const int radius =
        std::max(2L, std::min(bounds.right - bounds.left, bounds.bottom - bounds.top) / 5);
    const int sign = forward ? 1 : -1;
    POINT points[3];
    if (vertical) {
        points[0] = {x - radius, y - sign * radius / 2};
        points[1] = {x + radius, y - sign * radius / 2};
        points[2] = {x, y + sign * radius};
    } else {
        points[0] = {x - sign * radius / 2, y - radius};
        points[1] = {x - sign * radius / 2, y + radius};
        points[2] = {x + sign * radius, y};
    }
    const auto brush = CreateSolidBrush(color);
    const auto old_brush = SelectObject(dc, brush);
    const auto old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, points, 3);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(brush);
}
bool draw_scrollbar(HWND window, HDC dc, const RECT& outer, LONG object, const Palette& colors,
                    RECT& screen) {
    SCROLLBARINFO bar{};
    bar.cbSize = sizeof(bar);
    if (!GetScrollBarInfo(window, object, &bar) || (bar.rgstate[0] & STATE_SYSTEM_INVISIBLE))
        return false;
    if (bar.rcScrollBar.right <= bar.rcScrollBar.left ||
        bar.rcScrollBar.bottom <= bar.rcScrollBar.top)
        return false;
    screen = bar.rcScrollBar;
    RECT area = screen;
    OffsetRect(&area, -outer.left, -outer.top);
    const bool vertical = object == OBJID_VSCROLL;
    fill(dc, area, colors.surface);
    RECT first = area, last = area;
    if (vertical) {
        first.bottom = first.top + bar.dxyLineButton;
        last.top = last.bottom - bar.dxyLineButton;
    } else {
        first.right = first.left + bar.dxyLineButton;
        last.left = last.right - bar.dxyLineButton;
    }
    triangle(dc, first, vertical, false, colors.muted);
    triangle(dc, last, vertical, true, colors.muted);
    if (bar.xyThumbBottom > bar.xyThumbTop &&
        !(bar.rgstate[3] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_UNAVAILABLE))) {
        RECT thumb = area;
        if (vertical) {
            thumb.top = area.top + bar.xyThumbTop;
            thumb.bottom = area.top + bar.xyThumbBottom;
            InflateRect(&thumb, -2, -1);
        } else {
            thumb.left = area.left + bar.xyThumbTop;
            thumb.right = area.left + bar.xyThumbBottom;
            InflateRect(&thumb, -1, -2);
        }
        POINT cursor{};
        GetCursorPos(&cursor);
        cursor.x -= outer.left;
        cursor.y -= outer.top;
        const bool hot = PtInRect(&thumb, cursor);
        fill(dc, thumb, hot ? colors.accent : colors.muted);
    }
    return true;
}
void paint_scrollbars(HWND window, const Palette& colors) {
    if (colors.contrast || !IsWindowVisible(window))
        return;
    const auto style = GetWindowLongPtrW(window, GWL_STYLE);
    if (!(style & (WS_HSCROLL | WS_VSCROLL)))
        return;
    RECT outer{};
    GetWindowRect(window, &outer);
    const auto dc = GetWindowDC(window);
    if (!dc)
        return;
    const int saved = SaveDC(dc);
    RECT vertical{}, horizontal{};
    const bool has_vertical =
        (style & WS_VSCROLL) && draw_scrollbar(window, dc, outer, OBJID_VSCROLL, colors, vertical);
    const bool has_horizontal =
        (style & WS_HSCROLL) &&
        draw_scrollbar(window, dc, outer, OBJID_HSCROLL, colors, horizontal);
    if (has_vertical && has_horizontal) {
        RECT corner{vertical.left - outer.left, horizontal.top - outer.top,
                    vertical.right - outer.left, horizontal.bottom - outer.top};
        fill(dc, corner, colors.surface);
    }
    RestoreDC(dc, saved);
    ReleaseDC(window, dc);
}

// Report/list views (Sessions, Probe Tools and the generic file browser) retain
// their real Windows items, image lists, virtual callbacks and accessibility.
// Drawing a selected row explicitly avoids a native theme mixing our white
// foreground with a light system selection surface. No item state is changed.
bool paint_selected_list(NMLVCUSTOMDRAW& draw, const Palette& colors) {
    const auto window = draw.nmcd.hdr.hwndFrom;
    const int row = static_cast<int>(draw.nmcd.dwItemSpec);
    if (!(ListView_GetItemState(window, row, LVIS_SELECTED) & LVIS_SELECTED))
        return false;
    RECT bounds{};
    if (!ListView_GetItemRect(window, row, &bounds, LVIR_BOUNDS))
        return false;
    const auto view = GetWindowLongPtrW(window, GWL_STYLE) & LVS_TYPEMASK;
    if (view == LVS_REPORT) {
        RECT client{};
        GetClientRect(window, &client);
        bounds.left = client.left;
        bounds.right = client.right;
    }
    const auto dc = draw.nmcd.hdc;
    const int saved = SaveDC(dc);
    fill(dc, bounds, colors.selected_background);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colors.selected_foreground);
    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)));
    const auto header = ListView_GetHeader(window);
    const int columns =
        view == LVS_REPORT && header ? std::clamp(Header_GetItemCount(header), 1, 32) : 1;
    for (int column = 0; column < columns; ++column) {
        RECT rectangle{};
        if (column == 0) {
            if (!ListView_GetItemRect(window, row, &rectangle, LVIR_LABEL))
                continue;
        } else if (!ListView_GetSubItemRect(window, row, column, LVIR_LABEL, &rectangle))
            continue;
        wchar_t text[2048]{};
        LVITEMW item{};
        item.iSubItem = column;
        item.pszText = text;
        item.cchTextMax = 2048;
        SendMessageW(window, LVM_GETITEMTEXTW, row, reinterpret_cast<LPARAM>(&item));
        rectangle.left += 4;
        rectangle.right -= 4;
        DrawTextW(dc, text, -1, &rectangle,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    LVITEMW item{};
    item.mask = LVIF_IMAGE;
    item.iItem = row;
    const auto images =
        ListView_GetImageList(window, view == LVS_ICON ? LVSIL_NORMAL : LVSIL_SMALL);
    if (images && SendMessageW(window, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&item)) &&
        item.iImage >= 0) {
        RECT icon{};
        if (ListView_GetItemRect(window, row, &icon, LVIR_ICON))
            ImageList_Draw(images, item.iImage, dc, icon.left, icon.top, ILD_TRANSPARENT);
    }
    if (GetFocus() == window && (ListView_GetItemState(window, row, LVIS_FOCUSED) & LVIS_FOCUSED))
        DrawFocusRect(dc, &bounds);
    RestoreDC(dc, saved);
    return true;
}
LRESULT CALLBACK theme_proc(HWND window, UINT message, WPARAM first, LPARAM second, UINT_PTR id,
                            DWORD_PTR value) {
    auto* colors = reinterpret_cast<Palette*>(value);
    const auto copied_colors = *colors;
    if (message == WM_NOTIFY && second && !copied_colors.contrast) {
        auto* header = reinterpret_cast<NMHDR*>(second);
        wchar_t name[32]{};
        GetClassNameW(header->hwndFrom, name, 32);
        if (header->code == NM_CUSTOMDRAW && lstrcmpW(name, WC_LISTVIEWW) == 0) {
            auto& draw = *reinterpret_cast<NMLVCUSTOMDRAW*>(second);
            if (draw.nmcd.dwDrawStage == CDDS_PREPAINT)
                return DefSubclassProc(window, message, first, second) | CDRF_NOTIFYITEMDRAW;
            if (draw.nmcd.dwDrawStage == CDDS_ITEMPREPAINT &&
                paint_selected_list(draw, copied_colors))
                return CDRF_SKIPDEFAULT;
        }
    }
    if (message == WM_NCDESTROY) {
        KillTimer(window, tracking_timer);
        RemoveWindowSubclass(window, theme_proc, id);
        const auto result = DefSubclassProc(window, message, first, second);
        delete colors;
        return result;
    }
    if (message == WM_TIMER && first == tracking_timer) {
        paint_scrollbars(window, copied_colors);
        return 0;
    }
    // Windows keeps its real scrollbar hit testing, capture, keyboard and
    // accessibility. During its modal thumb loop a short-lived timer repaints
    // our colors after native tracking paints; there is no idle timer.
    if (message == WM_NCLBUTTONDOWN && (first == HTVSCROLL || first == HTHSCROLL) &&
        !copied_colors.contrast)
        SetTimer(window, tracking_timer, 16, nullptr);
    const auto result = DefSubclassProc(window, message, first, second);
    if (message == WM_NCLBUTTONDOWN || message == WM_CAPTURECHANGED)
        KillTimer(window, tracking_timer);
    if (message == WM_PAINT || message == WM_NCPAINT || message == WM_SIZE ||
        message == WM_THEMECHANGED || message == WM_NCMOUSEMOVE || message == WM_NCMOUSELEAVE ||
        message == WM_NCLBUTTONDOWN || message == WM_VSCROLL || message == WM_HSCROLL)
        if (IsWindow(window))
            paint_scrollbars(window, copied_colors);
    return result;
}
void apply(HWND window, const Palette& colors) {
    wchar_t name[64]{};
    GetClassNameW(window, name, 64);
    const bool combo = lstrcmpW(name, L"ComboBox") == 0;
    auto popup = [&] {
        COMBOBOXINFO info{};
        info.cbSize = sizeof(info);
        if (combo && GetComboBoxInfo(window, &info) && info.hwndList && info.hwndList != window)
            apply(info.hwndList, colors);
    };
    DWORD_PTR stored = 0;
    if (GetWindowSubclass(window, theme_proc, theme_subclass, &stored)) {
        if (*reinterpret_cast<Palette*>(stored) == colors) {
            popup();
            return;
        }
        *reinterpret_cast<Palette*>(stored) = colors;
    } else {
        auto* value = new (std::nothrow) Palette(colors);
        if (!value)
            return;
        if (!SetWindowSubclass(window, theme_proc, theme_subclass,
                               reinterpret_cast<DWORD_PTR>(value))) {
            delete value;
            return;
        }
    }
    if (api().allow)
        api().allow(window, colors.dark && !colors.contrast);
    SetWindowTheme(window,
                   colors.contrast ? nullptr
                   : colors.dark   ? (combo ? L"DarkMode_CFD" : L"DarkMode_Explorer")
                                   : L"Explorer",
                   nullptr);
    SendMessageW(window, WM_THEMECHANGED, 0, 0);
    if (!IsWindow(window))
        return;
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
    popup();
}
} // namespace
void prepare_native_appearance(const ViewAppearance& appearance) {
    const int mode = appearance.high_contrast ? 0 : appearance.dark ? 2 : 3;
    if (api().previous_mode == mode)
        return;
    api().previous_mode = mode;
    if (api().preferred)
        api().preferred(mode);
    if (api().flush)
        api().flush();
}
void apply_native_appearance(wxWindow& window, const ViewAppearance& appearance) {
    const auto handle = static_cast<HWND>(window.GetHandle());
    if (!handle)
        return;
    const auto colors = palette(appearance);
    apply(handle, colors);
    EnumChildWindows(
        handle,
        [](HWND child, LPARAM value) -> BOOL {
            apply(child, *reinterpret_cast<const Palette*>(value));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&colors));
}
void apply_menu_appearance(wxMenu& menu, const ViewAppearance& appearance) {
    for (auto* item : menu.GetMenuItems()) {
        if (auto* themed = dynamic_cast<MenuItem*>(item))
            themed->appearance = appearance;
        item->SetBackgroundColour(appearance.input);
        item->SetTextColour(appearance.foreground);
        if (item->GetSubMenu())
            apply_menu_appearance(*item->GetSubMenu(), appearance);
    }
}
} // namespace audiohole::plug_board::desktop
