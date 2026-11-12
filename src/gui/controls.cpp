#include "gui/controls.h"
#include "gui/native_theme.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#include <wx/dcbuffer.h>
#include <wx/filename.h>
#include <wx/generic/msgdlgg.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/stdpaths.h>

namespace audiohole::gui {
// Route wheel gestures to the nearest scrolling container. Never fall through
// to the native combo procedure, which can change a focused selection silently.
void forward_dropdown_wheel(wxWindow& control, wxMouseEvent& event) {
    for (auto* parent = control.GetParent(); parent; parent = parent->GetParent()) {
        if (!dynamic_cast<wxScrolledWindow*>(parent))
            continue;
        wxMouseEvent routed(event);
        routed.SetEventObject(parent);
        routed.SetPosition(parent->ScreenToClient(control.ClientToScreen(event.GetPosition())));
        parent->GetEventHandler()->ProcessEvent(routed);
        return;
    }
}
namespace {
bool native_dropdown_wheel(wxWindow& control, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message != WM_MOUSEWHEEL && message != WM_MOUSEHWHEEL)
        return false;
    wxMouseEvent event(wxEVT_MOUSEWHEEL);
    event.m_wheelRotation = GET_WHEEL_DELTA_WPARAM(wparam);
    event.m_wheelDelta = WHEEL_DELTA;
    UINT lines = 3;
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    event.m_linesPerAction = lines == WHEEL_PAGESCROLL ? 3 : static_cast<int>(lines);
    event.m_wheelAxis =
        message == WM_MOUSEHWHEEL ? wxMOUSE_WHEEL_HORIZONTAL : wxMOUSE_WHEEL_VERTICAL;
    event.SetPosition(control.ScreenToClient(wxPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))));
    forward_dropdown_wheel(control, event);
    return true;
}
constexpr UINT_PTR dropdown_wheel_subclass = 0x4148574c;
LRESULT CALLBACK dropdown_wheel_proc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam,
                                     UINT_PTR id, DWORD_PTR owner) {
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(handle, dropdown_wheel_proc, id);
    else if (native_dropdown_wheel(*reinterpret_cast<wxWindow*>(owner), message, wparam, lparam))
        return 0;
    return DefSubclassProc(handle, message, wparam, lparam);
}
void protect_dropdown_children(wxWindow& owner) {
    COMBOBOXINFO info{};
    info.cbSize = sizeof(info);
    const auto handle = static_cast<HWND>(owner.GetHandle());
    if (!GetComboBoxInfo(handle, &info))
        return;
    for (const auto child : {info.hwndList, info.hwndItem})
        if (child && child != handle)
            SetWindowSubclass(child, dropdown_wheel_proc, dropdown_wheel_subclass,
                              reinterpret_cast<DWORD_PTR>(&owner));
}
bool contrast_active() {
    HIGHCONTRASTW settings{};
    settings.cbSize = sizeof(settings);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(settings), &settings, 0) &&
           (settings.dwFlags & HCF_HIGHCONTRASTON);
}
bool paint_selection(wxWindow& control, const wxString& text, WXDRAWITEMSTRUCT* item) {
    const auto& draw = *reinterpret_cast<DRAWITEMSTRUCT*>(item);
    const bool selected = (draw.itemState & ODS_SELECTED) != 0;
    const bool disabled = (draw.itemState & ODS_DISABLED) != 0 || !control.IsEnabled();
    const bool dark = control.GetBackgroundColour().GetLuminance() < 0.5;
    const bool system = contrast_active();
    const auto background = system ? wxSystemSettings::GetColour(selected ? wxSYS_COLOUR_HIGHLIGHT
                                                                          : wxSYS_COLOUR_WINDOW)
                            : selected ? palette::selection(dark)
                                       : control.GetBackgroundColour();
    const auto foreground =
        system             ? wxSystemSettings::GetColour(selected ? wxSYS_COLOUR_HIGHLIGHTTEXT
                                                                  : wxSYS_COLOUR_WINDOWTEXT)
        : disabled         ? wxColour(dark ? 155 : 105, dark ? 166 : 105, dark ? 179 : 105)
        : selected && dark ? *wxWHITE
                           : control.GetForegroundColour();
    const auto saved = SaveDC(draw.hDC);
    auto rgb = [](const wxColour& c) { return RGB(c.Red(), c.Green(), c.Blue()); };
    const auto brush = CreateSolidBrush(rgb(background));
    FillRect(draw.hDC, &draw.rcItem, brush);
    DeleteObject(brush);
    SetBkMode(draw.hDC, TRANSPARENT);
    SetTextColor(draw.hDC, rgb(foreground));
    auto rect = draw.rcItem;
    rect.left += control.FromDIP(3);
    DrawTextW(draw.hDC, text.wc_str(), static_cast<int>(text.length()), &rect,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (draw.itemState & ODS_FOCUS)
        DrawFocusRect(draw.hDC, &draw.rcItem);
    RestoreDC(draw.hDC, saved);
    return true;
}
bool measure_selection(wxWindow& control, WXMEASUREITEMSTRUCT* item) {
    auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(item);
    measure->itemHeight = static_cast<UINT>(control.GetCharHeight() + control.FromDIP(6));
    return true;
}
} // namespace
ThemedChoice::ThemedChoice(wxWindow* parent, wxWindowID id) {
    Create(parent, id);
    protect_dropdown_children(*this);
}
WXDWORD ThemedChoice::MSWGetStyle(long style, WXDWORD* extra) const {
    return wxChoice::MSWGetStyle(style, extra) | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
}
bool ThemedChoice::MSWOnMeasure(WXMEASUREITEMSTRUCT* item) {
    return measure_selection(*this, item);
}
bool ThemedChoice::MSWOnDraw(WXDRAWITEMSTRUCT* item) {
    const auto id = reinterpret_cast<DRAWITEMSTRUCT*>(item)->itemID;
    return paint_selection(*this, id < GetCount() ? GetString(id) : wxString{}, item);
}
WXLRESULT ThemedChoice::MSWWindowProc(WXUINT message, WXWPARAM wparam, WXLPARAM lparam) {
    if (native_dropdown_wheel(*this, message, wparam, lparam))
        return 0;
    if ((message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC ||
         message == WM_CTLCOLORLISTBOX) &&
        GetBackgroundColour().IsOk()) {
        const auto background = GetBackgroundColour();
        const auto foreground = IsEnabled() ? GetForegroundColour()
                                : contrast_active()
                                    ? wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT)
                                    : wxColour(150, 160, 175);
        input_brush_ = wxBrush(background);
        const auto dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, RGB(background.Red(), background.Green(), background.Blue()));
        SetTextColor(dc, RGB(foreground.Red(), foreground.Green(), foreground.Blue()));
        return reinterpret_cast<WXLRESULT>(input_brush_.GetResourceHandle());
    }
    return wxChoice::MSWWindowProc(message, wparam, lparam);
}
ThemedCombo::ThemedCombo(wxWindow* parent, wxWindowID id) {
    Create(parent, id, {}, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxTE_PROCESS_ENTER);
    protect_dropdown_children(*this);
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        const int key = event.GetKeyCode();
        if (!event.AltDown() && (key == WXK_UP || key == WXK_DOWN) && !disabled_items.empty()) {
            const int step = key == WXK_UP ? -1 : 1;
            int index = GetSelection() + step;
            while (index >= 0 && index < static_cast<int>(GetCount()) && !item_enabled(index))
                index += step;
            if (index >= 0 && index < static_cast<int>(GetCount())) {
                SetSelection(index);
                wxCommandEvent selected(wxEVT_COMBOBOX, GetId());
                selected.SetEventObject(this);
                selected.SetInt(index);
                GetEventHandler()->ProcessEvent(selected);
            }
            return;
        }
        event.Skip();
    });
}
WXDWORD ThemedCombo::MSWGetStyle(long style, WXDWORD* extra) const {
    return wxComboBox::MSWGetStyle(style, extra) | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
}
bool ThemedCombo::MSWOnMeasure(WXMEASUREITEMSTRUCT* item) {
    return measure_selection(*this, item);
}
bool ThemedCombo::MSWOnDraw(WXDRAWITEMSTRUCT* item) {
    auto draw = *reinterpret_cast<DRAWITEMSTRUCT*>(item);
    if (!item_enabled(draw.itemID))
        draw.itemState |= ODS_DISABLED;
    return paint_selection(*this, draw.itemID < GetCount() ? GetString(draw.itemID) : wxString{},
                           reinterpret_cast<WXDRAWITEMSTRUCT*>(&draw));
}
WXLRESULT ThemedCombo::MSWWindowProc(WXUINT message, WXWPARAM wparam, WXLPARAM lparam) {
    if (native_dropdown_wheel(*this, message, wparam, lparam))
        return 0;
    if ((message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC ||
         message == WM_CTLCOLORLISTBOX) &&
        GetBackgroundColour().IsOk()) {
        const auto background = GetBackgroundColour();
        const auto foreground = IsEnabled() ? GetForegroundColour()
                                : contrast_active()
                                    ? wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT)
                                    : wxColour(150, 160, 175);
        input_brush_ = wxBrush(background);
        const auto dc = reinterpret_cast<HDC>(wparam);
        SetBkColor(dc, RGB(background.Red(), background.Green(), background.Blue()));
        SetTextColor(dc, RGB(foreground.Red(), foreground.Green(), foreground.Blue()));
        return reinterpret_cast<WXLRESULT>(input_brush_.GetResourceHandle());
    }
    return wxComboBox::MSWWindowProc(message, wparam, lparam);
}
bool ThemedList::MSWOnDraw(WXDRAWITEMSTRUCT* item) {
    auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(item);
    const auto id = draw->itemID;
    if (id >= status_colours.size())
        return paint_selection(*this, id < GetCount() ? GetString(id) : wxString{}, item);
    auto text_rect = *draw;
    text_rect.rcItem.left += FromDIP(24);
    paint_selection(*this, "", item);
    paint_selection(*this, GetString(id), reinterpret_cast<WXDRAWITEMSTRUCT*>(&text_rect));
    const int saved = SaveDC(draw->hDC);
    const auto c = contrast_active() ? GetForegroundColour() : status_colours[id];
    SetTextColor(draw->hDC, RGB(c.Red(), c.Green(), c.Blue()));
    SetBkMode(draw->hDC, TRANSPARENT);
    auto rect = draw->rcItem;
    rect.right = rect.left + FromDIP(24);
    DrawTextW(draw->hDC, L"●", 1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RestoreDC(draw->hDC, saved);
    return true;
}
void theme_dialog(wxWindow* window) {
    // wxGenericMessageDialog constructs its native children lazily in ShowModal.
    // Apply only once InitDialog has created them, before the first visible paint.
    if (!window->GetHandle()) {
        window->Bind(wxEVT_INIT_DIALOG, [window](wxInitDialogEvent& event) {
            theme_dialog(window);
            event.Skip();
        });
        return;
    }
    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    const bool contrast = contrast_active(), dark = !light && !contrast;
    std::function<void(wxWindow*)> paint = [&](wxWindow* child) {
        if (!contrast) {
            child->SetBackgroundColour(dark ? palette::charcoal() : wxColour(245, 247, 250));
            child->SetForegroundColour(dark ? *wxWHITE : *wxBLACK);
        }
        for (auto* next : child->GetChildren())
            paint(next);
    };
    paint(window);
    native_theme(window, dark, contrast);
    const BOOL title_dark = dark ? TRUE : FALSE;
    (void)DwmSetWindowAttribute(static_cast<HWND>(window->GetHandle()), 20, &title_dark,
                                sizeof(title_dark));
}
bool confirm_exit(wxWindow* parent) {
    wxGenericMessageDialog dialog(parent, "Exit AudioHole?", "Exit AudioHole?",
                                  wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    dialog.SetYesNoLabels("Exit", "Cancel");
    dialog.SetEscapeId(wxID_NO);
    theme_dialog(&dialog);
    return dialog.ShowModal() == wxID_YES;
}
int themed_message_box(const wxString& message, const wxString& caption, long style,
                       wxWindow* parent) {
    wxGenericMessageDialog dialog(parent, message, caption, style);
    theme_dialog(&dialog);
    const auto result = dialog.ShowModal();
    return result == wxID_YES      ? wxYES
           : result == wxID_NO     ? wxNO
           : result == wxID_CANCEL ? wxCANCEL
                                   : wxOK;
}

namespace {
void bind_feedback(wxWindow* button) {
    button->Bind(wxEVT_ENTER_WINDOW, [button](wxMouseEvent& event) {
        button->Refresh();
        event.Skip();
    });
    button->Bind(wxEVT_LEAVE_WINDOW, [button](wxMouseEvent& event) {
        button->Refresh();
        event.Skip();
    });
    // F1 exposes the same explanation without requiring a pointing device.
    button->Bind(wxEVT_HELP, [button](wxHelpEvent&) {
        if (button->GetToolTip())
            themed_message_box(button->GetToolTip()->GetTip(), button->GetLabel(),
                               wxOK | wxICON_INFORMATION, button);
    });
}
// wx 3.2's MSW themed owner-draw path can paint a white hot-state surface
// despite SetThemeEnabled(false). Paint the complete matched foreground/background
// pair ourselves. Windows still supplies focus, disabled, pressed and command state.
// Borrow the supplied HDC/font; restore selected objects and release owned brushes.
bool paint_button(wxWindow& button, WXDRAWITEMSTRUCT* item, bool selected) {
    const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(item);
    const bool enabled = !(draw->itemState & ODS_DISABLED);
    const bool pressed = (draw->itemState & ODS_SELECTED) != 0;
    const bool dark = button.GetParent()->GetBackgroundColour().GetLuminance() < 0.5;
    const bool hover = button.GetScreenRect().Contains(wxGetMousePosition());
    wxColour background = button.GetBackgroundColour();
    if (!enabled)
        background = dark ? wxColour(30, 36, 43) : wxColour(224, 226, 230);
    if (enabled && (hover || pressed))
        background = background == palette::mint() ? background.ChangeLightness(pressed ? 85 : 110)
                     : dark ? wxColour(pressed ? 40 : 52, pressed ? 59 : 75, pressed ? 78 : 96)
                            : wxColour(pressed ? 186 : 215, pressed ? 204 : 229, 244);
    const wxColour foreground = !enabled
                                    ? wxColour(dark ? 136 : 110, dark ? 147 : 110, dark ? 163 : 110)
                                    : button.GetForegroundColour();
    auto rgb = [](const wxColour& colour) {
        return RGB(colour.Red(), colour.Green(), colour.Blue());
    };
    HBRUSH brush = CreateSolidBrush(rgb(background));
    FillRect(draw->hDC, &draw->rcItem, brush);
    DeleteObject(brush);
    brush = CreateSolidBrush(rgb(selected ? wxColour(75, 146, 210)
                                 : dark   ? wxColour(76, 88, 104)
                                          : wxColour(170, 183, 198)));
    auto border_rect = draw->rcItem;
    for (int pixel = 0; pixel < std::max(2, button.FromDIP(2)); ++pixel) {
        FrameRect(draw->hDC, &border_rect, brush);
        InflateRect(&border_rect, -1, -1);
    }
    DeleteObject(brush);
    const int saved = SaveDC(draw->hDC);
    SelectObject(draw->hDC, reinterpret_cast<HFONT>(SendMessageW(
                                static_cast<HWND>(button.GetHandle()), WM_GETFONT, 0, 0)));
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, rgb(foreground));
    RECT text_rect = draw->rcItem;
    InflateRect(&text_rect, -button.FromDIP(8), -button.FromDIP(3));
    if (pressed)
        OffsetRect(&text_rect, 1, 1);
    DrawTextW(draw->hDC, button.GetLabel().wc_str(), -1, &text_rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if ((draw->itemState & ODS_FOCUS) && !(draw->itemState & ODS_NOFOCUSRECT)) {
        RECT focus = draw->rcItem;
        InflateRect(&focus, -3, -3);
        DrawFocusRect(draw->hDC, &focus);
    }
    RestoreDC(draw->hDC, saved);
    return true;
}
} // namespace
SurfacePanel::SurfacePanel(wxWindow* parent, const wxString& role)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxTAB_TRAVERSAL | wxFULL_REPAINT_ON_RESIZE) {
    SetName(role);
    // A rounded right/bottom edge moves during resizing. Repaint the whole
    // buffered surface, not only newly exposed strips, to erase its old stroke.
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        const auto role = GetName();
        const auto border = role == "capture-card" || role == "commentary-card" ? palette::magenta()
                            : role == "playback-card"                           ? palette::mint()
                                                      : wxColour(81, 108, 120);
        const int thickness = std::max(2, FromDIP(2));
        dc.SetPen(wxPen(use_system_colours ? GetForegroundColour() : border, thickness));
        dc.SetBrush(wxBrush(GetBackgroundColour()));
        dc.DrawRoundedRectangle(GetClientRect().Deflate((thickness + 1) / 2), FromDIP(10));
    });
}
void SurfacePanel::set_content_sizer(wxSizer* contents) {
    auto* inset = new wxBoxSizer(wxVERTICAL);
    inset->Add(contents, 1, wxEXPAND | wxALL, FromDIP(12));
    SetSizer(inset);
}
ActionButton::ActionButton(wxWindow* parent, const wxString& title, long style, wxWindowID id)
    : wxButton(parent, id, title, wxDefaultPosition, wxDefaultSize, style) {
    SetToolTip(button_help(title));
    SetMinSize(wxSize(-1, FromDIP(32)));
    MakeOwnerDrawn();
    bind_feedback(this);
}
bool ActionButton::MSWOnDraw(WXDRAWITEMSTRUCT* item) {
    return use_native_colours ? wxButton::MSWOnDraw(item) : paint_button(*this, item, false);
}
NavigationButton::NavigationButton(wxWindow* parent) : wxToggleButton(parent, wxID_ANY, "") {
    MakeOwnerDrawn();
    bind_feedback(this);
}
bool NavigationButton::MSWOnDraw(WXDRAWITEMSTRUCT* item) {
    return use_native_colours ? wxToggleButton::MSWOnDraw(item)
                              : paint_button(*this, item, GetValue());
}
void material_heading(wxWindow* parent, wxSizer* layout, const wxString& title, bool capture) {
    static const bool loaded = wxFont::AddPrivateFont(
        wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath() + wxFILE_SEP_PATH +
        "assets" + wxFILE_SEP_PATH + "ui" + wxFILE_SEP_PATH + "MaterialIcons-Regular.ttf");
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    if (loaded) {
        auto* icon =
            new wxStaticText(parent, wxID_ANY, wxString(wxUniChar(capture ? 0xe029 : 0xebbe)));
        icon->SetFont(wxFont(wxFontInfo(18).FaceName("Material Icons")));
        icon->SetName(capture ? "Microphone icon" : "Media output icon");
        icon->SetToolTip(title);
        row->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, parent->FromDIP(6));
    }
    label(parent, row, title)->SetFont(parent->GetFont().Bold());
    layout->Add(row, 0, wxEXPAND);
}
wxColour routing_colour(audiohole::State state) {
    switch (state) {
    case audiohole::State::Running:
        return wxColour(46, 175, 110);
    case audiohole::State::Failed:
        return wxColour(230, 83, 83);
    case audiohole::State::Degraded:
        return wxColour(234, 158, 52);
    case audiohole::State::Ready:
        return wxColour(100, 160, 224);
    case audiohole::State::Starting:
    case audiohole::State::Recovering:
    case audiohole::State::Stopping:
        return wxColour(220, 185, 75);
    default:
        return wxColour(145, 154, 168);
    }
}
// Explanations are stable technical copy, shared by hover and keyboard help.
// Keep entries aligned with the visible action labels; smoke checks cover every button.
wxString button_help(const wxString& title) {
    static const std::pair<const char*, const char*> help[] = {
        {"Start Routing", "Validate and activate the profile currently open in the editor."},
        {"Stop Routing", "Stop every direction and cancel pending startup and recovery."},
        {"Exit AudioHole",
         "Stop routing and exit completely. Closing the window instead hides it to the tray."},
        {"New", "Create a new profile without changing active routing."},
        {"Duplicate", "Copy this profile with new profile and hole identities."},
        {"Delete",
         "Delete the selected profile after confirmation. Active profiles cannot be deleted."},
        {"+ Add Hole", "Add a directional connection to this profile's editor."},
        {"Remove selected hole",
         "Remove this connection after confirmation. Apply to update active routing."},
        {"Save", "Save editor changes without changing the active routing plan."},
        {"Apply", "Validate, save and activate the edited profile."},
        {"Revert", "Discard unsaved edits and reload the saved profile after confirmation."},
        {"Reset to Automatic",
         "Reset this direction's format, channel mapping, access mode and buffer to Automatic. "
         "Endpoint assignments and the other direction stay unchanged. Save / Apply keeps the "
         "change."},
        {"Save device",
         "Update the display name and notes. Windows name and logical identity remain unchanged."},
        {"Export devices…",
         "Export saved device records, identities, notes, capabilities and available probe "
         "histories as a portable ZIP. Local Windows bindings are excluded."},
        {"Import devices…",
         "Preview a device bundle before adding records. Conflicting identities or filenames are "
         "not overwritten; imported devices need an Update scan to resolve local bindings."},
        {"Export profiles…",
         "Export all saved profiles and hole settings with their referenced device records. Save "
         "pending edits first; no runtime or startup authorization is exported."},
        {"Import profiles…",
         "Preview profiles and referenced devices before importing. Stable IDs, notes and settings "
         "are retained; conflicting existing records are never silently replaced."},
        {"Toggle availability",
         "Change device availability in this evaluation scenario to exercise reconnection."},
        {"Start scan", "Scan the selected scope. Advanced testing requests confirmation first."},
        {"Cancel scan", "Cancel the current scan while retaining existing device records."},
        {"Copy visible diagnostics",
         "Copy the currently filtered technical events to the clipboard."},
        {"Export visible diagnostics…",
         "Save the currently filtered technical events to a text file."},
        {"Clear diagnostics log",
         "Clear all in-memory diagnostic events, including filtered entries. Routing, profiles and "
         "settings are unchanged; no files are deleted."},
        {"Reload personality files",
         "Reload optional commentary from the personalities folder beside this executable."},
        {"Load custom personality…", "Validate and select a local JSON commentary catalog. Invalid "
                                     "files leave a complete available package selected."},
        {"Preview startup",
         "Exercise immediate or delayed profile activation without registering Windows startup."},
        {"Cancel pending activation",
         "Cancel the startup countdown and disarm pending activation."},
        {"Replay Tutorial", "Restart the welcome/tutorial after confirmation; your "
                            "saved configuration is preserved."},
        {"DFA", "Working as intended."},
        {"Test beep", "Test only the explicitly selected playback destination. Cancel stops the "
                      "test immediately."},
        {"Cancel beep", "End the current destination test."},
        {"Forward failure", "Exercise a forward-direction failure in this evaluation scenario."},
        {"Return failure",
         "Exercise a return-direction failure while preserving the forward direction."},
        {"Recover", "Retry unavailable directions without restarting healthy paths."},
    };
    for (const auto& [label, explanation] : help)
        if (title == wxString::FromUTF8(label))
            return wxString::FromUTF8(explanation);
    return {}; // A missing entry is caught by the native GUI smoke check.
}
} // namespace audiohole::gui
