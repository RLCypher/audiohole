#include "gui/controls.h"
#include <windows.h>
#include <wx/dcbuffer.h>

namespace audiohole::gui {
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
            wxMessageBox(button->GetToolTip()->GetTip(), button->GetLabel(),
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
    const bool dark = button.GetBackgroundColour().GetLuminance() < 0.5;
    const bool hover = button.GetScreenRect().Contains(wxGetMousePosition());
    wxColour background = button.GetBackgroundColour();
    if (!enabled)
        background = dark ? wxColour(30, 36, 43) : wxColour(224, 226, 230);
    if (enabled && (hover || pressed))
        background = dark ? wxColour(pressed ? 40 : 52, pressed ? 59 : 75, pressed ? 78 : 96)
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
    FrameRect(draw->hDC, &draw->rcItem, brush);
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
        dc.SetPen(wxPen(use_system_colours ? GetForegroundColour() : border, FromDIP(1)));
        dc.SetBrush(wxBrush(GetBackgroundColour()));
        dc.DrawRoundedRectangle(GetClientRect().Deflate(1), FromDIP(10));
    });
}
void SurfacePanel::set_content_sizer(wxSizer* contents) {
    auto* inset = new wxBoxSizer(wxVERTICAL);
    inset->Add(contents, 1, wxEXPAND | wxALL, FromDIP(12));
    SetSizer(inset);
}
ActionButton::ActionButton(wxWindow* parent, const wxString& title)
    : wxButton(parent, wxID_ANY, title) {
    SetToolTip(button_help(title));
    SetMinSize(wxSize(-1, FromDIP(32)));
    bind_feedback(this);
}
bool ActionButton::MSWOnDraw(WXDRAWITEMSTRUCT* item) {
    return use_native_colours ? wxButton::MSWOnDraw(item) : paint_button(*this, item, false);
}
NavigationButton::NavigationButton(wxWindow* parent) : wxToggleButton(parent, wxID_ANY, "") {
    bind_feedback(this);
}
bool NavigationButton::MSWOnDraw(WXDRAWITEMSTRUCT* item) {
    return use_native_colours ? wxToggleButton::MSWOnDraw(item)
                              : paint_button(*this, item, GetValue());
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
        {"Arm Ready", "Replace the active plan and wait for unavailable devices to return."},
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
        {"Save device",
         "Update the display name and notes. Windows name and logical identity remain unchanged."},
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
                                     "files fall back to Vanilla."},
        {"Preview startup",
         "Exercise immediate or delayed profile activation without registering Windows startup."},
        {"Cancel pending activation",
         "Cancel the startup countdown and disarm pending activation."},
        {"Replay Tutorial", "Reopen the setup tutorial. This build displays a placeholder; your "
                            "session is unchanged."},
        {"DFA", "Working as intended."},
        {"Test Beep", "Test only the explicitly selected playback destination. Cancel stops the "
                      "test immediately."},
        {"Cancel Test Beep", "End the current destination test."},
        {"Forward failure", "Exercise a forward-direction failure in this evaluation scenario."},
        {"Reverse failure",
         "Exercise a reverse-direction failure while preserving the forward direction."},
        {"Recover", "Retry unavailable directions without restarting healthy paths."},
    };
    for (const auto& [label, explanation] : help)
        if (title == wxString::FromUTF8(label))
            return wxString::FromUTF8(explanation);
    return {}; // A missing entry is caught by the native GUI smoke check.
}
} // namespace audiohole::gui
