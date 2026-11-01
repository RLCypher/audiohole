#pragma once
#include "gui/branding.h"
#include "model/controller.h"
#include "platform/windows/com_apartment.h"
#include "presentation/messages.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <windows.h>
#include <wx/clipbrd.h>
#include <wx/collpane.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/scrolwin.h>
#include <wx/simplebook.h>
#include <wx/slider.h>
#include <wx/snglinst.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/taskbar.h>
#include <wx/tglbtn.h>
#include <wx/wx.h>

namespace audiohole::gui {
class Frame;
// Owns the shell icon. Frame outlives this object; every action uses the same controller.
class Tray final : public wxTaskBarIcon {
  public:
    explicit Tray(Frame& frame);
    wxMenu* CreatePopupMenu() override;
    void update(const audiohole::Snapshot&);

  private:
    Frame& frame_;
    audiohole::State last_ = audiohole::State::Stopped;
    std::string tooltip_;
    int icon_size_ = 0;
    TrayIconCache icons_;
};
// Non-owning widget handles. wx parent/child ownership destroys controls with Frame.
// ID and format vectors map visible selections to typed controller values.
struct DirectionWidgets {
    wxStaticBoxSizer* group{};
    wxComboBox *source{}, *destination{};
    wxChoice *format{}, *mode{};
    wxSpinCtrl* buffer{};
    int acknowledged_buffer_ms = 6; // GUI-only deduplication, reset when loading a direction.
    wxStaticText* evidence{};
    std::vector<std::string> sources, destinations;
    std::vector<audiohole::Format> formats;
};
// Native view/controller adapter. Owns the controller and tray; wx owns child widgets.
// Editor drafts are separate from saved session records and the copied live plan.
// Timers advance deterministic fixture time only; no I/O or audio work is done here.
class Frame final : public wxFrame {
  public:
    // Takes exclusive controller ownership. All widgets and commands are GUI-thread only.
    explicit Frame(std::unique_ptr<audiohole::Controller> controller, bool smoke);
    ~Frame() override {
        RemovePropW(static_cast<HWND>(GetHandle()), L"AudioHole.GuiWindow");
        timer_.Stop();
        if (tray_)
            tray_->RemoveIcon();
    }
    audiohole::Controller& controller() {
        return *controller_;
    }
    void open() {
        Show();
        Iconize(false);
        Raise();
        SetFocus();
    }
    void exit() {
        timer_.Stop();
        controller_->stop();
        end_observation();
        if (tray_)
            tray_->RemoveIcon();
        Destroy();
    }
    void show_testing_notice() {
        testing_notice();
    }
    void tray_activate(const std::string& id) {
        show_result(controller_->activate(id));
        refresh();
    }
    void tray_start() {
        if (!controller_->snapshot().active_profile.empty())
            tray_activate(controller_->snapshot().active_profile);
        else
            activate(false);
    }
    // Reads one controller snapshot; changes commentary only for new event occurrences.
    void refresh();
    // Internal developer check; dispatches native events and writes explicit evidence.
    bool smoke(const wxString& directory);

  private:
    std::unique_ptr<audiohole::Controller> controller_;
    std::unique_ptr<Tray> tray_;
    wxTimer timer_;
    bool smoke_ = false, loading_ = false, dirty_ = false;
    audiohole::Profile draft_;
    int hole_index_ = -1;
    std::string viewed_, device_id_;
    std::uint64_t seen_event_ = 0, occurrence_ = 0;
    std::string personality_ = "default";
    std::string personality_fallback_note_;
    std::filesystem::path personalities_directory_;
    std::vector<std::string> personality_ids_;
    presentation::Message last_message_ = presentation::Message::Ready;
    presentation::Messages messages_;
    wxSimplebook* notebook_{};
    std::vector<wxToggleButton*> navigation_;
    bool dark_appearance_ = true;
    wxChoice* theme_choice_{};
    wxStaticText *personality_notice_{}, *fallback_notice_{};
    wxSlider* dfa_slider_{};
    wxTextCtrl* dfa_value_{};
    wxSpinButton* dfa_stepper_{};
    wxStaticText* dfa_feedback_{};
    int dfa_hundredths_ = 0; // Last valid normalized transient value; never serialized.
    bool operational_warning_visible_ = false; // Header-copy protection, not engine health.
    std::optional<int> last_dfa_settled_;
    std::vector<wxWindow*> dfa_controls_;
    AgentDisplay* agent_display_{};
    wxPanel* status_dot_{};
    wxStaticText *status_{}, *technical_{}, *commentary_{}, *path_status_{}, *beep_status_{},
        *activity_{}, *device_detail_{};
    wxButton *start_{}, *stop_{}, *dfa_{}, *scan_start_{}, *scan_cancel_{};
    wxListBox *profiles_{}, *holes_{}, *devices_{};
    wxTextCtrl *profile_name_{}, *profile_notes_{}, *hole_name_{}, *hole_notes_{}, *device_name_{},
        *device_notes_{}, *device_search_{}, *console_{}, *log_search_{};
    wxCheckBox *enabled_{}, *duplex_{}, *monitor_{};
    wxChoice *activity_direction_{}, *personality_choice_{}, *destination_test_{}, *scan_mode_{},
        *scan_scope_{}, *severity_{}, *startup_{}, *startup_target_{}, *notifications_{};
    wxSpinCtrl* delay_{};
    wxGauge* scan_progress_{};
    DirectionWidgets forward_, reverse_;
    std::vector<std::string> profile_ids_, device_ids_, beep_ids_;
    std::string last_log_;
    // Retain unwrapped copy so resizing never rerolls commentary or accumulates
    // line breaks. These are transient view strings, not controller state.
    std::string status_copy_ = "Stopped", technical_copy_, commentary_copy_, fallback_copy_;
    void layout_header();
    // Builders only construct widgets/bind events; they never issue routing commands.
    void build_profiles();
    void build_hole_diagnostics(wxWindow* parent, wxSizer* layout);
    void build_devices();
    void build_scan();
    void build_settings();
    // File I/O occurs only on explicit GUI load/reload, never during polling.
    void reload_personalities();
    void select_personality(const std::string& id);
    void sync_personality_choices();
    void settle_dfa(int hundredths);
    void commit_dfa_text();
    void adjust_dfa(int delta);
    // Use the existing catalog; preserve an operational header warning by placing
    // DFA feedback beside its controls instead. No controller commands or logging.
    void present_dfa(presentation::Message);
    void update_status_indicator();
    // Appearance touches only native view resources; no controller commands.
    void apply_appearance();
    void select_page(std::size_t page, bool focus = false);
    void update_navigation();
    WXLRESULT MSWWindowProc(WXUINT, WXWPARAM, WXLPARAM) override;
    void build_direction(wxWindow*, wxSizer*, DirectionWidgets&, const wxString&);
    // Rebuilds evidence while retaining explicit unsupported overrides for correction.
    void populate_direction(DirectionWidgets&, const audiohole::Direction&);
    void read_direction(DirectionWidgets&, audiohole::Direction&);
    // Acknowledge a committed editor change once; never changes controller/live state.
    void acknowledge_buffer(DirectionWidgets&);
    void select_profile(const std::string&);
    void load_holes();
    void load_hole(int);
    void update_beep_destinations();
    // Copies editor fields into draft_ only, never into the active plan.
    void capture_draft();
    // Confirms a potentially destructive editor navigation; does not mutate state.
    bool discard();
    void save(bool);
    void activate(bool);
    void populate_devices();
    void load_device();
    // Called on page change, hide/minimize, Stop, and shutdown. No sample ownership.
    void end_observation();
    void present(presentation::Message);
    void show_result(const audiohole::Result&);
    void testing_notice();
    void export_log();
};

} // namespace audiohole::gui
