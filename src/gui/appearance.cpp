#include "gui/controls.h"
#include "gui/frame.h"
#include <dwmapi.h>
#include <wx/settings.h>
#include <wx/tglbtn.h>

namespace audiohole::gui {
namespace {
// Read-only OS preferences. No registry values or system settings are modified.
bool high_contrast() {
    HIGHCONTRASTW settings{};
    settings.cbSize = sizeof(settings);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(settings), &settings, 0) &&
           (settings.dwFlags & HCF_HIGHCONTRASTON);
}
bool system_dark() {
    DWORD light = 1, size = sizeof(light);
    const auto result = RegGetValueW(
        HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return result == ERROR_SUCCESS && light == 0;
}
void apply_palette(wxWindow* window, bool dark, bool contrast) {
    const wxColour surface = dark ? wxColour(24, 27, 33) : wxColour(245, 247, 250);
    const wxColour input = dark ? wxColour(38, 43, 52) : wxColour(255, 255, 255);
    const wxColour foreground = dark ? wxColour(237, 240, 246) : wxColour(23, 30, 40);
    // Native visual styles can ignore explicit backgrounds. Disable them on
    // application controls so their text/background remain a matched pair.
    // High contrast instead uses Windows' own colors and native styles.
    window->SetThemeEnabled(contrast);
    if (auto* button = dynamic_cast<ActionButton*>(window))
        button->use_native_colours = contrast;
    if (auto* button = dynamic_cast<NavigationButton*>(window))
        button->use_native_colours = contrast;
    const bool editor = dynamic_cast<wxTextCtrl*>(window) || dynamic_cast<wxComboBox*>(window) ||
                        dynamic_cast<wxChoice*>(window) || dynamic_cast<wxListBox*>(window) ||
                        dynamic_cast<wxSpinCtrl*>(window) ||
                        dynamic_cast<wxSpinCtrlDouble*>(window);
    window->SetBackgroundColour(
        contrast ? wxSystemSettings::GetColour(editor ? wxSYS_COLOUR_WINDOW : wxSYS_COLOUR_BTNFACE)
        : editor ? input
                 : surface);
    window->SetForegroundColour(
        contrast
            ? wxSystemSettings::GetColour(editor ? wxSYS_COLOUR_WINDOWTEXT : wxSYS_COLOUR_BTNTEXT)
            : foreground);
    for (auto* child : window->GetChildren())
        apply_palette(child, dark, contrast);
}
} // namespace
void Frame::select_page(std::size_t page, bool focus) {
    if (page >= notebook_->GetPageCount())
        return;
    notebook_->SetSelection(page);
    update_navigation();
    if (focus)
        navigation_.at(page)->SetFocus();
}
void Frame::update_navigation() {
    const bool contrast = high_contrast();
    const auto selected = notebook_->GetSelection();
    for (std::size_t index = 0; index < navigation_.size(); ++index) {
        auto* tab = navigation_[index];
        const bool active = static_cast<int>(index) == selected;
        tab->SetValue(active);
        tab->SetToolTip(wxS("Open ") + notebook_->GetPageText(index) +
                        wxS(". Use arrows or Ctrl+Tab to change pages."));
        // Selected wording and bold weight remain identifiable without color.
        tab->SetLabel(notebook_->GetPageText(index) +
                      (active ? wxString(wxS(" [selected]")) : wxString{}));
        auto font = tab->GetFont();
        font.SetWeight(active ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
        tab->SetFont(font);
        tab->SetBackgroundColour(contrast
                                     ? wxSystemSettings::GetColour(active ? wxSYS_COLOUR_HIGHLIGHT
                                                                          : wxSYS_COLOUR_BTNFACE)
                                 : active           ? wxColour(20, 92, 160)
                                 : dark_appearance_ ? wxColour(59, 66, 79)
                                                    : wxColour(220, 226, 235));
        tab->SetForegroundColour(
            contrast ? wxSystemSettings::GetColour(active ? wxSYS_COLOUR_HIGHLIGHTTEXT
                                                          : wxSYS_COLOUR_BTNTEXT)
            : active || dark_appearance_ ? *wxWHITE
                                         : wxColour(20, 28, 40));
        tab->Refresh();
    }
}
void Frame::apply_appearance() {
    const bool contrast = high_contrast();
    // Keep these indices aligned with the Dark/Light/Follow System selector.
    constexpr int dark_choice = 0;
    constexpr int system_choice = 2;
    dark_appearance_ =
        !contrast && (theme_choice_->GetSelection() == dark_choice ||
                      (theme_choice_->GetSelection() == system_choice && system_dark()));
    apply_palette(this, dark_appearance_, contrast);
    // Attribute 20 is DWMWA_USE_IMMERSIVE_DARK_MODE. Older Windows builds can
    // reject it; this optional title-bar enhancement never blocks the GUI.
    constexpr DWORD immersive_dark_mode = 20;
    const BOOL dark_title = dark_appearance_ ? TRUE : FALSE;
    (void)DwmSetWindowAttribute(static_cast<HWND>(GetHandle()), immersive_dark_mode, &dark_title,
                                sizeof(dark_title));
    update_navigation();
    update_status_indicator();
    Refresh();
}
// Color supplements the adjacent canonical state text; it never replaces it.
// High contrast uses the system text color instead of imposing a color scheme.
void Frame::update_status_indicator() {
    const auto state = controller_->snapshot().state;
    status_dot_->SetForegroundColour(high_contrast()
                                         ? wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)
                                         : routing_colour(state));
    status_dot_->SetToolTip(text(std::string("Routing status: ") + audiohole::name(state)));
    status_dot_->Refresh();
}
WXLRESULT Frame::MSWWindowProc(WXUINT message, WXWPARAM wparam, WXLPARAM lparam) {
    const auto result = wxFrame::MSWWindowProc(message, wparam, lparam);
    if (message == WM_SETTINGCHANGE && theme_choice_)
        CallAfter([this] {
            if (!IsBeingDeleted())
                apply_appearance();
        });
    return result;
}
} // namespace audiohole::gui
