#include "gui/controls.h"
#include "gui/frame.h"
#include "gui/native_theme.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
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
    const wxColour surface = dark ? palette::charcoal() : wxColour(245, 247, 250);
    const wxColour input = dark ? palette::input() : wxColour(255, 255, 255);
    const wxColour foreground = dark ? *wxWHITE : *wxBLACK;
    // Pair application text/background first. The native-theme pass then applies
    // scrollbar/arrow styles; app-owned buttons keep their matched custom paint.
    // High contrast uses Windows' own colors and native styles throughout.
    window->SetThemeEnabled(contrast);
    if (auto* panel = dynamic_cast<SurfacePanel*>(window))
        panel->use_system_colours = contrast;
    if (auto* button = dynamic_cast<ActionButton*>(window))
        button->use_native_colours = contrast;
    if (auto* button = dynamic_cast<NavigationButton*>(window))
        button->use_native_colours = contrast;
    const bool editor = dynamic_cast<wxTextCtrl*>(window) || dynamic_cast<wxComboBox*>(window) ||
                        dynamic_cast<wxChoice*>(window) || dynamic_cast<wxListBox*>(window) ||
                        dynamic_cast<wxSpinCtrl*>(window) ||
                        dynamic_cast<wxSpinCtrlDouble*>(window) ||
                        dynamic_cast<DeviceList*>(window);
    window->SetBackgroundColour(
        contrast ? wxSystemSettings::GetColour(editor ? wxSYS_COLOUR_WINDOW : wxSYS_COLOUR_BTNFACE)
        : editor ? input
                 : surface);
    window->SetForegroundColour(
        contrast
            ? wxSystemSettings::GetColour(editor ? wxSYS_COLOUR_WINDOWTEXT : wxSYS_COLOUR_BTNTEXT)
            : foreground);
    if (!contrast && window->GetName() == "brand-wordmark")
        window->SetForegroundColour(palette::mint());
    if (!contrast && window->GetName() == "brand-wordmark-hole")
        window->SetForegroundColour(palette::magenta());
    for (auto* child : window->GetChildren())
        apply_palette(child, dark, contrast);
    if (auto* list = dynamic_cast<DeviceList*>(window)) {
        list->GetMainWindow()->SetBackgroundColour(
            contrast ? wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW) : input);
        list->GetMainWindow()->SetForegroundColour(
            contrast ? wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT) : foreground);
    }
}
} // namespace
void Frame::select_page(std::size_t page, bool focus) {
    if (page >= notebook_->GetPageCount())
        return;
    if (page != 0)
        tutorial_scroll_duration_ = 0;
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
        tab->UnsetToolTip();
        // Brackets and bold weight identify selection without adding a status suffix.
        const auto title = notebook_->GetPageText(index);
        tab->SetLabel(active ? wxS("[") + title + wxS("]") : title);
        auto font = tab->GetFont();
        font.SetWeight(active ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
        tab->SetFont(font);
        tab->SetBackgroundColour(contrast
                                     ? wxSystemSettings::GetColour(active ? wxSYS_COLOUR_HIGHLIGHT
                                                                          : wxSYS_COLOUR_BTNFACE)
                                 : active           ? palette::mint()
                                 : dark_appearance_ ? wxColour(59, 66, 79)
                                                    : *wxWHITE);
        tab->SetForegroundColour(
            contrast           ? wxSystemSettings::GetColour(active ? wxSYS_COLOUR_HIGHLIGHTTEXT
                                                                    : wxSYS_COLOUR_BTNTEXT)
            : active           ? wxColour(7, 24, 19)
            : dark_appearance_ ? *wxWHITE
                               : wxColour(20, 28, 40));
        tab->Refresh();
    }
}
void Frame::apply_appearance() {
    avatar_host_->first_frame_only(preferences_->values().low_motion);
    BOOL client_animation = TRUE;
    if (avatar_host_ && SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &client_animation, 0))
        avatar_host_->reduced_motion(preferences_->values().low_motion ||
                                     client_animation == FALSE);
    const bool contrast = high_contrast();
    // Keep these indices aligned with the Dark/Light/Follow System selector.
    constexpr int dark_choice = 0;
    constexpr int system_choice = 2;
    dark_appearance_ =
        !contrast && (theme_choice_->GetSelection() == dark_choice ||
                      (theme_choice_->GetSelection() == system_choice && system_dark()));
    prepare_native_theme(dark_appearance_, contrast);
    apply_palette(this, dark_appearance_, contrast);
    native_theme(this, dark_appearance_, contrast);
    for (auto* control : tutorial_choices_) {
        auto* button = static_cast<ActionButton*>(control);
        button->outline_colour = control->GetLabel() == "End tutorial"
                                     ? palette::magenta()
                                     : control->GetBackgroundColour();
    }
    if (!contrast) {
        // Explicit colors require the documented unthemed native gauge path.
        const auto handle = static_cast<HWND>(scan_progress_->GetHandle());
        SetWindowTheme(handle, L"", L"");
        SendMessageW(handle, PBM_SETBARCOLOR, 0, RGB(6, 176, 37));
        const auto background = dark_appearance_ ? palette::input() : wxColour(230, 233, 238);
        SendMessageW(handle, PBM_SETBKCOLOR, 0,
                     RGB(background.Red(), background.Green(), background.Blue()));
    }
    if (!contrast) {
        for (auto* primary : {start_, profile_save_}) {
            primary->SetBackgroundColour(palette::mint());
            primary->SetForegroundColour(wxColour(7, 24, 19));
        }
        personality_heading_->SetForegroundColour(palette::magenta());
    }
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
    if (message == WM_COPYDATA && controller_) {
        const auto action = decode_shell_action(reinterpret_cast<const COPYDATASTRUCT*>(lparam));
        if (!action || IsBeingDeleted())
            return FALSE;
        if ((*action == ShellAction::Start || *action == ShellAction::Stop) &&
            !controller_->snapshot().capabilities.routing)
            return FALSE;
        // Acknowledge admission before running a menu or beginning shutdown, so
        // Explorer's short-lived relay never waits for UI/worker cleanup.
        CallAfter([this, action = *action] { perform_shell_action(action); });
        return TRUE;
    }
    if (message == WM_SYSCOMMAND) {
        const auto command = static_cast<UINT>(wparam) & 0xFFF0;
        if (command == shell_controls_command || command == shell_exit_command) {
            CallAfter([this, command] {
                perform_shell_action(command == shell_controls_command ? ShellAction::Controls
                                                                       : ShellAction::Exit);
            });
            return 0;
        }
    }
    const auto result = wxFrame::MSWWindowProc(message, wparam, lparam);
    if (message == WM_DEVICECHANGE && !smoke_ && controller_ &&
        controller_->snapshot().scanner.available) {
        // Debounce notifications; all enumeration remains on the owned worker.
        device_change_pending_ = true;
        device_change_after_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
    }
    if (message == WM_SETTINGCHANGE && theme_choice_)
        CallAfter([this] {
            if (!IsBeingDeleted())
                apply_appearance();
        });
    return result;
}
} // namespace audiohole::gui
