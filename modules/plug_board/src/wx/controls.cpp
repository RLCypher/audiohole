#include "controls.hpp"
#include "dialogs.hpp"
#include "native_theme.hpp"
#include <wx/listctrl.h>
#include <wx/msw/dc.h>

// Windows base types must precede the optional control/theme headers.
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <commctrl.h>
#include <dwmapi.h>

namespace audiohole::plug_board::desktop {

namespace {
constexpr UINT_PTR popup_wheel_subclass = 0x50425748;
LRESULT CALLBACK popup_wheel(HWND window, UINT message, WPARAM first, LPARAM second, UINT_PTR id,
                             DWORD_PTR) {
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(window, popup_wheel, id);
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
        return 0;
    return DefSubclassProc(window, message, first, second);
}
void protect_popup(HWND window) {
    COMBOBOXINFO info{};
    info.cbSize = sizeof(info);
    if (GetComboBoxInfo(window, &info))
        for (const auto child : {info.hwndList, info.hwndItem})
            if (child && child != window)
                SetWindowSubclass(child, popup_wheel, popup_wheel_subclass, 0);
}
} // namespace

Appearance appearance_for(const wxString& preference) {
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    const bool high_contrast =
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
        (contrast.dwFlags & HCF_HIGHCONTRASTON);
    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    Appearance appearance;
    appearance.high_contrast = high_contrast;
    appearance.dark = preference == "Dark" || (preference == "Follow System" && light == 0);
    if (high_contrast) {
        appearance.surface = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
        appearance.input = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        appearance.foreground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        appearance.muted = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
        appearance.accent = appearance.foreground;
    } else {
        appearance.surface = appearance.dark ? wxColour(10, 19, 25) : wxColour(245, 247, 250);
        appearance.input = appearance.dark ? wxColour(21, 35, 43) : *wxWHITE;
        appearance.foreground = appearance.dark ? *wxWHITE : *wxBLACK;
        appearance.muted = appearance.dark ? wxColour(167, 184, 195) : wxColour(75, 90, 103);
        appearance.accent = appearance.dark ? wxColour(80, 237, 178) : wxColour(15, 101, 75);
    }
    return appearance;
}

double contrast_ratio(const wxColour& first, const wxColour& second) {
    const auto luminance = [](const wxColour& color) {
        const auto linear = [](unsigned char channel) {
            const double value = channel / 255.0;
            return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(color.Red()) + 0.7152 * linear(color.Green()) +
               0.0722 * linear(color.Blue());
    };
    const auto left = luminance(first), right = luminance(second);
    return (std::max(left, right) + 0.05) / (std::min(left, right) + 0.05);
}
ItemColors item_colors(const Appearance& appearance, bool selected, bool enabled) {
    if (appearance.high_contrast)
        return {
            wxSystemSettings::GetColour(selected ? wxSYS_COLOUR_HIGHLIGHT : wxSYS_COLOUR_WINDOW),
            wxSystemSettings::GetColour(selected  ? wxSYS_COLOUR_HIGHLIGHTTEXT
                                        : enabled ? wxSYS_COLOUR_WINDOWTEXT
                                                  : wxSYS_COLOUR_GRAYTEXT)};
    const auto background = selected
                                ? (appearance.dark ? wxColour(25, 86, 72) : wxColour(204, 233, 223))
                                : appearance.input;
    auto foreground = enabled || selected ? appearance.foreground : appearance.muted;
    if (contrast_ratio(background, foreground) < 4.5)
        foreground = contrast_ratio(background, *wxWHITE) > contrast_ratio(background, *wxBLACK)
                         ? *wxWHITE
                         : *wxBLACK;
    return {background, foreground};
}

ListBox::ListBox(wxWindow* parent, wxWindowID id, const wxSize& size)
    : wxListBox(parent, id, wxDefaultPosition, size, 0, nullptr, wxLB_SINGLE | wxLB_OWNERDRAW) {}
bool ListBox::MSWOnMeasure(WXMEASUREITEMSTRUCT* value) {
    reinterpret_cast<MEASUREITEMSTRUCT*>(value)->itemHeight =
        static_cast<UINT>(GetCharHeight() + FromDIP(6));
    return true;
}
bool ListBox::MSWOnDraw(WXDRAWITEMSTRUCT* value) {
    const auto& item = *reinterpret_cast<DRAWITEMSTRUCT*>(value);
    const auto colors = item_colors(appearance, (item.itemState & ODS_SELECTED) != 0,
                                    IsEnabled() && !(item.itemState & ODS_DISABLED));
    const auto brush = CreateSolidBrush(
        RGB(colors.background.Red(), colors.background.Green(), colors.background.Blue()));
    FillRect(item.hDC, &item.rcItem, brush);
    DeleteObject(brush);
    const int saved = SaveDC(item.hDC);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC,
                 RGB(colors.foreground.Red(), colors.foreground.Green(), colors.foreground.Blue()));
    SelectObject(item.hDC, reinterpret_cast<HFONT>(
                               SendMessageW(static_cast<HWND>(GetHandle()), WM_GETFONT, 0, 0)));
    const auto text = item.itemID < GetCount() ? GetString(item.itemID) : wxString{};
    auto rectangle = item.rcItem;
    rectangle.left += FromDIP(4);
    DrawTextW(item.hDC, text.wc_str(), static_cast<int>(text.length()), &rectangle,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (item.itemState & ODS_FOCUS)
        DrawFocusRect(item.hDC, &item.rcItem);
    RestoreDC(item.hDC, saved);
    return true;
}
MenuItem::MenuItem(wxMenu* parent, int id, const wxString& label, wxMenu* submenu, wxItemKind kind)
    : wxMenuItem(parent, id, label, {}, kind, submenu) {
    SetOwnerDrawn(true);
}
void MenuItem::GetColourToUse(wxODStatus status, wxColour& foreground, wxColour& background) const {
    const auto colors =
        item_colors(appearance, (status & wxODSelected) != 0, (status & wxODDisabled) == 0);
    foreground = colors.foreground;
    background = colors.background;
}

void apply_appearance(wxWindow& window, const Appearance& appearance) {
    prepare_native_appearance(appearance);
    const bool editor = dynamic_cast<wxTextCtrl*>(&window) || dynamic_cast<wxListCtrl*>(&window) ||
                        dynamic_cast<wxListBox*>(&window) || dynamic_cast<wxChoice*>(&window);
    window.SetBackgroundColour(editor ? appearance.input : appearance.surface);
    window.SetForegroundColour(appearance.foreground);
    if (auto* button = dynamic_cast<Button*>(&window)) {
        button->high_contrast = appearance.high_contrast;
        button->SetBackgroundColour(appearance.high_contrast
                                        ? wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE)
                                        : appearance.input);
    }
    if (auto* choice = dynamic_cast<Choice*>(&window))
        choice->high_contrast = appearance.high_contrast;
    if (auto* list = dynamic_cast<ListBox*>(&window))
        list->appearance = appearance;
    apply_native_appearance(window, appearance);
    for (auto* child : window.GetChildren()) {
        apply_appearance(*child, appearance);
    }
    if (auto* frame = dynamic_cast<wxTopLevelWindow*>(&window)) {
        const BOOL dark = appearance.dark && !appearance.high_contrast;
        DwmSetWindowAttribute(static_cast<HWND>(frame->GetHandle()), 20, &dark, sizeof(dark));
    }
    window.Refresh();
}

Appearance appearance_for_window(wxWindow* window) {
    const auto* top = window ? wxGetTopLevelParent(window) : nullptr;
    if (!top || !top->GetBackgroundColour().IsOk())
        return appearance_for("Follow System");
    return appearance_for(top->GetBackgroundColour().GetLuminance() < 0.5 ? "Dark" : "Light");
}
Button::Button(wxWindow* parent, const wxString& label, const wxString& help, wxWindowID id)
    : wxButton(parent, id, label) {
    SetName(label);
    SetToolTip(help);
    SetMinSize(wxSize(-1, FromDIP(32)));
    Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& event) {
        Refresh();
        event.Skip();
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& event) {
        Refresh();
        event.Skip();
    });
    Bind(wxEVT_HELP, [this](wxHelpEvent&) {
        if (GetToolTip())
            show_message(this, GetToolTip()->GetTip(), GetLabel(), wxOK | wxICON_INFORMATION,
                         appearance_for_window(this));
    });
}

bool Button::MSWOnDraw(WXDRAWITEMSTRUCT* item) {
    if (high_contrast) {
        return wxButton::MSWOnDraw(item);
    }
    // Paint a matched pair in hot/pressed/disabled states. Native wx/Windows
    // still provides keyboard, focus, accessibility and button command handling.
    const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(item);
    const bool enabled = !(draw->itemState & ODS_DISABLED);
    const bool pressed = (draw->itemState & ODS_SELECTED) != 0;
    const bool dark = GetParent()->GetBackgroundColour().GetLuminance() < 0.5;
    wxColour background = GetBackgroundColour();
    if (enabled && GetScreenRect().Contains(wxGetMousePosition())) {
        background = background.ChangeLightness(pressed ? 90 : dark ? 135 : 95);
    }
    wxColour foreground = enabled ? GetForegroundColour()
                          : dark  ? wxColour(140, 153, 166)
                                  : wxColour(112, 118, 126);
    if (!high_contrast && contrast_ratio(background, foreground) < 4.5)
        foreground = contrast_ratio(background, *wxWHITE) > contrast_ratio(background, *wxBLACK)
                         ? *wxWHITE
                         : *wxBLACK;
    auto native_color = [](const wxColour& color) {
        return RGB(color.Red(), color.Green(), color.Blue());
    };
    auto brush = CreateSolidBrush(native_color(background));
    FillRect(draw->hDC, &draw->rcItem, brush);
    DeleteObject(brush);
    brush = CreateSolidBrush(native_color(dark ? wxColour(76, 88, 104) : wxColour(170, 183, 198)));
    auto border = draw->rcItem;
    for (int pixel = 0; pixel < std::max(2, FromDIP(2)); ++pixel) {
        FrameRect(draw->hDC, &border, brush);
        InflateRect(&border, -1, -1);
    }
    DeleteObject(brush);
    const int saved = SaveDC(draw->hDC);
    SelectObject(draw->hDC, reinterpret_cast<HFONT>(
                                SendMessageW(static_cast<HWND>(GetHandle()), WM_GETFONT, 0, 0)));
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, native_color(foreground));
    auto text = draw->rcItem;
    const int padding = GetClientSize().x < FromDIP(80) ? FromDIP(3) : FromDIP(8);
    InflateRect(&text, -padding, -FromDIP(3));
    if (GetBitmap().IsOk()) {
        const auto bitmap = GetBitmap();
        wxDCTemp dc(reinterpret_cast<WXHDC>(draw->hDC));
        dc.DrawBitmap(bitmap, text.left, (text.top + text.bottom - bitmap.GetHeight()) / 2, true);
        text.left += bitmap.GetWidth() + FromDIP(6);
    }
    DrawTextW(draw->hDC, GetLabel().wc_str(), -1, &text,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if ((draw->itemState & ODS_FOCUS) && !(draw->itemState & ODS_NOFOCUSRECT)) {
        auto focus = draw->rcItem;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(draw->hDC, &focus);
    }
    RestoreDC(draw->hDC, saved);
    return true;
}

Choice::Choice(wxWindow* parent, wxWindowID id) {
    Create(parent, id);
    protect_popup(static_cast<HWND>(GetHandle()));
}
WXDWORD Choice::MSWGetStyle(long style, WXDWORD* extra) const {
    return wxChoice::MSWGetStyle(style, extra) | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
}
wxSize Choice::DoGetBestSize() const {
    auto size = wxChoice::DoGetBestSize();
    size.y = std::max(size.y, GetCharHeight() + FromDIP(12));
    return size;
}
bool Choice::MSWOnMeasure(WXMEASUREITEMSTRUCT* value) {
    reinterpret_cast<MEASUREITEMSTRUCT*>(value)->itemHeight =
        static_cast<UINT>(GetCharHeight() + FromDIP(6));
    return true;
}
bool Choice::MSWOnDraw(WXDRAWITEMSTRUCT* value) {
    // Keep the real native combo box, keyboard behavior and accessibility, while
    // painting both the closed selection and every popup item with app colors.
    const auto& item = *reinterpret_cast<DRAWITEMSTRUCT*>(value);
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0 || !IsEnabled();
    auto appearance = appearance_for(GetBackgroundColour().GetLuminance() < 0.5 ? "Dark" : "Light");
    appearance.input = GetBackgroundColour();
    appearance.foreground = GetForegroundColour();
    appearance.high_contrast = high_contrast;
    const auto colors = item_colors(appearance, selected, !disabled);
    const auto background = colors.background, foreground = colors.foreground;
    const auto native = [](const wxColour& color) {
        return RGB(color.Red(), color.Green(), color.Blue());
    };
    const auto brush = CreateSolidBrush(native(background));
    FillRect(item.hDC, &item.rcItem, brush);
    DeleteObject(brush);
    const int saved = SaveDC(item.hDC);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, native(foreground));
    auto bounds = item.rcItem;
    bounds.left += FromDIP(4);
    const auto text = item.itemID < GetCount() ? GetString(item.itemID) : wxString{};
    DrawTextW(item.hDC, text.wc_str(), static_cast<int>(text.length()), &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (item.itemState & ODS_FOCUS)
        DrawFocusRect(item.hDC, &item.rcItem);
    RestoreDC(item.hDC, saved);
    return true;
}
void Choice::paint_arrow() {
    if (high_contrast || !GetHandle())
        return;
    const auto window = static_cast<HWND>(GetHandle());
    COMBOBOXINFO info{};
    info.cbSize = sizeof(info);
    if (!GetComboBoxInfo(window, &info))
        return;
    const auto dc = GetDC(window);
    if (!dc)
        return;
    const auto background = GetBackgroundColour(), foreground = GetForegroundColour();
    const auto brush =
        CreateSolidBrush(RGB(background.Red(), background.Green(), background.Blue()));
    FillRect(dc, &info.rcButton, brush);
    DeleteObject(brush);
    const int x = (info.rcButton.left + info.rcButton.right) / 2,
              y = (info.rcButton.top + info.rcButton.bottom) / 2;
    const int radius = FromDIP(3);
    POINT points[3]{{x - radius, y - radius / 2}, {x + radius, y - radius / 2}, {x, y + radius}};
    const auto arrow =
        CreateSolidBrush(RGB(foreground.Red(), foreground.Green(), foreground.Blue()));
    const auto old_brush = SelectObject(dc, arrow),
               old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, points, 3);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(arrow);
    ReleaseDC(window, dc);
}
WXLRESULT Choice::MSWWindowProc(WXUINT message, WXWPARAM first, WXLPARAM second) {
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
        return 0;
    }
    if (message == CB_SETITEMHEIGHT && first == static_cast<WXWPARAM>(-1))
        second = std::max(static_cast<int>(second), GetCharHeight() + FromDIP(12) - 6);
    if ((message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC ||
         message == WM_CTLCOLORLISTBOX) &&
        GetBackgroundColour().IsOk()) {
        input_brush = wxBrush(GetBackgroundColour());
        const auto dc = reinterpret_cast<HDC>(first);
        const auto background = GetBackgroundColour(), foreground = GetForegroundColour();
        SetBkColor(dc, RGB(background.Red(), background.Green(), background.Blue()));
        SetTextColor(dc, RGB(foreground.Red(), foreground.Green(), foreground.Blue()));
        return reinterpret_cast<WXLRESULT>(input_brush.GetResourceHandle());
    }
    const auto result = wxChoice::MSWWindowProc(message, first, second);
    if (message == WM_THEMECHANGED && GetHandle())
        SendMessageW(static_cast<HWND>(GetHandle()), CB_SETITEMHEIGHT, static_cast<WPARAM>(-1),
                     GetCharHeight() + FromDIP(12) - 6);
    if (message == CB_SHOWDROPDOWN && GetHandle()) {
        protect_popup(static_cast<HWND>(GetHandle()));
        // Some Windows builds create the popup HWND lazily, after the original
        // theme traversal. Theme that actual popup before its first paint.
        apply_native_appearance(
            *this, appearance_for(GetBackgroundColour().GetLuminance() < 0.5 ? "Dark" : "Light"));
    }
    if (message == WM_PAINT || message == WM_NCPAINT || message == WM_MOUSEMOVE ||
        message == WM_MOUSELEAVE || message == WM_ENABLE || message == WM_SETFOCUS ||
        message == WM_KILLFOCUS)
        paint_arrow();
    return result;
}

} // namespace audiohole::plug_board::desktop
