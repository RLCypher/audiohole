#pragma once
#include "audiohole/avatar/wx_panel.hpp"
#include "gui/branding.h"
#include "gui/device_list.h"
#include "gui/seconds_field.h"
#include "gui/shell_actions.h"
#include "model/build_preferences.h"
#include "model/controller.h"
#include "model/format_assessment.h"
#include "model/tutorial.h"
#include "platform/windows/com_apartment.h"
#include "presentation/avatar_host.h"
#include "presentation/device_events.h"
#include "presentation/messages.h"
#include "presentation/sleep_commentary.h"
#include <algorithm>
#include <array>
#include <chrono>
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
#include <wx/eventfilter.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/scrolwin.h>
#include <wx/simplebook.h>
#include <wx/slider.h>
#include <wx/snglinst.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
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
struct DiagnosticWidgets {
    wxPanel* panel{};
    wxButton* action{};
    wxGauge* meter{};
    std::string target;
    bool capture = false;
};
// A refresh preserves raw edits; explicit loads/resets replace only their section.
enum class DirectionFields { Preserve, Format, Timing, Routing, All };
struct DirectionWidgets {
    DiagnosticWidgets capture_check, playback_check;
    wxStaticBoxSizer* group{};
    wxStaticBoxSizer* tuning_group{};
    wxCollapsiblePane* tuning{};
    wxChoice* clock{};
    wxCheckBox *advanced_tuning{}, *exclusive_preferred{};
    wxComboBox *source{}, *destination{};
    wxChoice *format{}, *mode{}, *representation{};
    wxTextCtrl *rate{}, *channels{}, *source_channels{}, *destination_channels{}, *channel_map{};
    wxSpinCtrl* buffer{};
    std::array<wxTextCtrl*, 8> timings{};
    std::array<wxCollapsiblePane*, 3> sections{};
    std::array<wxStaticText*, 3> section_summaries{};
    wxButton*
        buffer_apply{}; // Commits one buffer preference; backend reports any live confirmation.
    wxStaticText* buffer_state{};
    int acknowledged_buffer_ms = 6; // GUI-only deduplication, reset when loading a direction.
    wxTextCtrl* information{};      // Fixed-height, wrapped, scrollable summary and evidence.
    wxStaticText *source_detail{}, *destination_detail{};
    wxString source_detail_copy, destination_detail_copy;
    std::vector<std::string> sources, destinations;
    std::string retained_source,
        retained_destination; // Hidden/missing saved bindings, never dropdown options.
    wxString retained_source_caption, retained_destination_caption;
    std::vector<audiohole::FormatCandidate> formats;
    bool manual_format = false;
};
// Native view/controller adapter. Owns the controller and tray; wx owns child widgets.
// Editor drafts are separate from saved session records and the copied live plan.
// The control timer advances fixture time; the avatar timer samples presentation.
// Neither performs audio work.
class Frame final : public wxFrame, private wxEventFilter {
  public:
    // Takes exclusive controller ownership. All widgets and commands are GUI-thread only.
    explicit Frame(std::unique_ptr<audiohole::Controller> controller, bool smoke,
                   std::shared_ptr<BuildPreferences> preferences = {},
                   std::function<void()> restart = {});
    ~Frame() override {
        wxEvtHandler::RemoveFilter(this);
        RemovePropW(static_cast<HWND>(GetHandle()), L"AudioHole.GuiWindow");
        timer_.Stop();
        tuning_timer_.Stop();
        stop_avatar();
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
    void request_exit();
    void exit();
    bool tutorial_exit_requested() const {
        return tutorial_exit_requested_;
    }
    bool show_onboarding_greeting(OnboardingOutcome);
    void show_testing_notice() {
        testing_notice();
    }
    void tray_activate(const std::string& id) {
        activate_saved_profile(id, false);
    }
    void tray_start() {
        if (!controller_->snapshot().active_profile.empty())
            tray_activate(controller_->snapshot().active_profile);
        else
            activate(false);
    }
    // Tray and taskbar share these actions and menus on the GUI thread.
    std::unique_ptr<wxMenu> shell_menu();
    void perform_shell_action(ShellAction);
    // Reads one controller snapshot; changes commentary only for new event occurrences.
    void refresh();
    // Internal developer check; dispatches native events and writes explicit evidence.
    void tutorial_replay_intro() {
        if (tutorial_) {
            tutorial_->remind("B012");
            render_tutorial();
        }
    }
    bool tutorial_smoke(const wxString& directory);
    bool engine_smoke(const wxString& directory);
    bool smoke(const wxString& directory);

  private:
    int FilterEvent(wxEvent& event) override;
    std::unique_ptr<audiohole::Controller> controller_;
    std::shared_ptr<BuildPreferences> preferences_;
    std::function<void()> restart_;
    bool tutorial_exit_requested_ = false;
    // Disposable native-smoke override; empty in every interactive host.
    std::filesystem::path tutorial_marker_override_;
    std::unique_ptr<Tray> tray_;
    wxTimer timer_;
    wxTimer avatar_timer_;
    bool tuning_reaction_reverse_ = false;
    wxTimer tuning_timer_; // One settled policy reaction; never a controller command.
    std::unique_ptr<presentation::AvatarHost> avatar_host_;
    std::vector<std::string> avatar_diagnostics_, last_avatar_diagnostics_;
    presentation::SleepCommentary sleep_commentary_{
        static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())};
    std::string sleep_copy_;
    std::uint64_t dream_occurrence_ = 0;
    void poll_avatar();
    void stop_avatar();
    bool smoke_ = false, loading_ = false, dirty_ = false;
    bool closing_ = false;
    void finish_exit();
    std::unique_ptr<Tutorial> tutorial_;
    wxPanel* avatar_region_{};
    wxBoxSizer* avatar_layout_{};
    wxPanel* tutorial_choices_panel_{};
    wxBoxSizer* tutorial_choices_layout_{};
    std::vector<wxButton*> tutorial_choices_;
    std::vector<wxWindow*> tutorial_highlights_, tutorial_reply_outlines_;
    wxPoint tutorial_scroll_from_, tutorial_scroll_to_;
    std::int64_t tutorial_scroll_started_ = 0;
    int tutorial_scroll_duration_ = 0;
    std::string tutorial_scroll_region_;
    void scroll_tutorial_to(wxWindow&, std::string_view region, int duration);
    bool advance_tutorial_scroll(std::int64_t now);
    wxWindow* tutorial_reply_arrow_{};
    std::string tutorial_rendered_, tutorial_choice_key_;
    std::uint64_t tutorial_seen_event_ = 0;
    bool tutorial_marker_saved_ = false, tutorial_completion_attempted_ = false;
    void tutorial_event(std::string_view);
    void render_tutorial();
    void skip_tutorial();
    bool finish_tutorial(OnboardingOutcome = OnboardingOutcome::Completed);
    bool enter_save_pending_ = false;
    void queue_field_save(bool device);
    bool profile_edits_pending();
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
    presentation::DeviceEvents device_events_;
    std::string device_event_copy_;
    wxSimplebook* notebook_{};
    std::vector<wxToggleButton*> navigation_;
    bool dark_appearance_ = true;
    wxChoice* theme_choice_{};
    wxCheckBox* hold_expressions_{};
    wxCheckBox *loop_until_event_{}, *low_motion_{};
    wxSpinCtrl* animation_loops_{};
    wxStaticText *personality_notice_{}, *fallback_notice_{};
    wxSlider* dfa_slider_{};
    wxTextCtrl* dfa_value_{};
    wxSpinButton* dfa_stepper_{};
    wxStaticText* dfa_feedback_{};
    int dfa_hundredths_ = 0; // Last valid normalized transient value; never serialized.
    bool operational_warning_visible_ = false; // Header-copy protection, not engine health.
    std::optional<int> last_dfa_settled_;
    std::vector<wxWindow*> dfa_controls_;
    avatar::Panel* agent_display_{};
    std::chrono::steady_clock::time_point bother_after_{};
    void bother(); // Avatar-click entry point; selected replies affect presentation only.
    wxScrolledWindow* commentary_panel_{};
    wxPanel* comment_surface_{};
    wxBoxSizer* routing_actions_{}; // Borrowed; natural button width bounds the commentary card.
    bool header_layout_pending_ = false;
    wxStaticText* personality_heading_{};
    wxPanel* status_dot_{};
    wxStaticText *status_{}, *technical_{}, *commentary_{}, *device_detail_{};
    wxTextCtrl* path_status_{};
    wxButton *start_{}, *stop_{}, *dfa_{}, *scan_start_{}, *scan_cancel_{};
    wxButton* profile_apply_{};
    wxButton *profile_save_{}, *profile_revert_{}, *profile_new_{}, *hole_new_{};
    wxButton *profile_remove_{}, *hole_remove_{}, *profiles_folder_{}, *devices_folder_{};
    wxPanel *profile_sidebar_{}, *profile_actions_{};
    wxStaticText *profile_edit_state_{}, *hole_heading_{};
    wxListBox *profiles_{}, *holes_{};
    DeviceList *capture_devices_{}, *playback_devices_{};
    wxTextCtrl *profile_notes_{}, *hole_notes_{}, *device_name_{}, *device_notes_{},
        *device_search_{}, *console_{}, *log_search_{};
    wxCheckBox *enabled_{}, *duplex_{};
    wxChoice* routing_engine_{};
    wxStaticText* routing_engine_notice_{};
    wxChoice *personality_choice_{}, *scan_mode_{}, *scan_scope_{}, *severity_{}, *startup_{},
        *startup_target_{}, *notifications_{};
    wxSpinCtrl* delay_{};
    wxGauge* scan_progress_{};
    wxButton* tutorial_end_{};
    DirectionWidgets forward_, reverse_;
    DiagnosticWidgets device_check_;
    DiagnosticWidgets* active_check_ = nullptr;
    wxPanel* performance_panel_{};
    wxSplitterWindow* performance_split_{};
    wxScrolledWindow* profile_editor_{};
    void build_device_check(wxWindow*, wxSizer*, DiagnosticWidgets&, bool capture,
                            const wxString& name);
    void refresh_device_checks();
    void start_device_check(DiagnosticWidgets&);
    std::vector<std::string> profile_ids_, capture_device_ids_, playback_device_ids_;
    wxChoice* device_view_{};
    wxTextCtrl* device_evidence_{};
    wxButton* device_save_{};
    wxSplitterWindow *profile_columns_{}, *profile_rows_{}, *profile_lower_rows_{},
        *device_columns_{}, *device_rows_{};
    std::vector<wxButton*> scanner_actions_;
    std::uint64_t device_inventory_revision_ = 0;
    std::string loaded_device_id_;
    std::shared_ptr<const DeviceInspection> device_edit_snapshot_;
    bool device_dirty_ = false, populating_devices_ = false;
    std::uint64_t device_save_generation_ = 0;
    bool scanner_clock_active_ = false;
    std::uint64_t bundle_generation_ = 0;
    bool bundle_inspecting_ = false, bundle_importing_ = false;
    std::chrono::steady_clock::time_point scanner_clock_started_{};
    wxTextCtrl* scan_state_{};
    wxSplitterWindow* scan_output_split_{};
    wxChoice* deep_mode_{};
    wxSpinCtrl* deep_attempts_{};
    SecondsField *deep_duration_{}, *deep_attempt_timeout_{}, *deep_device_timeout_{},
        *deep_system_timeout_{};
    wxTextCtrl* scan_context_{};
    std::chrono::steady_clock::time_point device_change_after_{};
    bool device_change_pending_ = false;
    std::string last_log_;
    // Retain unwrapped copy so resizing never rerolls commentary or accumulates
    // line breaks. These are transient view strings, not controller state.
    std::string status_copy_ = "Stopped", technical_copy_, commentary_copy_, fallback_copy_;
    void layout_header();
    // Builders only construct widgets/bind events; they never issue routing commands.
    void build_profiles();
    void initialize_shell_controls();
    // Context menus capture stable IDs; opening them never discards staged edits.
    std::unique_ptr<wxMenu> profile_menu(const std::string& id);
    std::unique_ptr<wxMenu> hole_menu(const std::string& id);
    void rename_profile(const std::string& name);
    void rename_hole(const std::string& name);
    void add_hole();
    void duplicate_hole();
    void remove_hole();
    void build_hole_diagnostics(wxWindow* parent, wxSizer* layout);
    void build_devices();
    void build_scan();
    void build_settings();
    void show_update_information();
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
    void tuning_changed(DirectionWidgets&);
    // Rebuilds evidence while retaining explicit unsupported overrides for correction.
    void populate_direction(DirectionWidgets&, const audiohole::Direction&,
                            DirectionFields load_settings = DirectionFields::All);
    bool validate_profile_inputs();
    void read_direction(DirectionWidgets&, audiohole::Direction&);
    void update_direction_details(DirectionWidgets&, const audiohole::Direction&);
    void layout_direction(DirectionWidgets&);
    void delete_selection(bool profile);
    void open_data_folder(bool profiles);
    // Acknowledge a committed editor change once; never changes controller/live state.
    void acknowledge_buffer(DirectionWidgets&);
    void apply_buffer(DirectionWidgets&);
    void select_profile(const std::string&);
    void load_holes();
    void load_hole(int);
    // Copies editor fields into draft_ only, never into the active plan.
    void capture_draft();
    // Confirms a potentially destructive editor navigation; does not mutate state.
    bool discard();
    void save(bool);
    void activate_saved_profile(const std::string& id, bool ready);
    void activate(bool);
    void populate_devices();
    void load_device();
    void constrain_profile_rows();
    void reset_layout();
    wxSizer* device_bundle_actions_{};
    bool select_device(const std::string&);
    std::unique_ptr<wxMenu> device_menu(const std::string&);
    void device_context(DeviceList*, const std::vector<std::string>&, wxContextMenuEvent&);
    void request_scan(bool deep, ScanScope, const std::string&);
    void scanner_action(const ScannerActionRequest&);
    void bundle_dialog(bool profiles, bool importing);
    // Called on page change, hide/minimize, Stop, and shutdown. No sample ownership.
    void end_observation();
    void present(presentation::Message);
    void show_result(const audiohole::Result&, bool operational = false);
    void testing_notice();
    void export_log();
    void update_console(const wxString&);
};

} // namespace audiohole::gui
