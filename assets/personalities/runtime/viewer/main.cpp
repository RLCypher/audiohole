#include "audiohole/avatar/wx_panel.hpp"
#include "proposed_adapter.hpp"
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <wx/dirdlg.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>

namespace av = audiohole::avatar;
namespace bridge = audiohole::avatar::integration;
using Json = nlohmann::json;
namespace {
std::string utf8(const wxString& text) {
    return text.ToUTF8().data();
}
std::filesystem::path native_path(const wxString& value) {
#ifdef _WIN32
    return std::filesystem::path(value.ToStdWstring());
#else
    return std::filesystem::path(utf8(value));
#endif
}
const std::vector<std::string> identities{"vanilla", "default", "uwu"};
const std::vector<std::string> states{
    "idle",     "happy",      "thinking",  "working",     "talking",     "confused", "concerned",
    "error",    "angry",      "surprised", "celebrating", "sleeping",    "smug",     "embarrassed",
    "laughing", "suspicious", "panicking", "bored",       "mischievous", "deadpan"};
const std::vector<audiohole::State> operational{
    audiohole::State::Stopped, audiohole::State::Ready,    audiohole::State::Starting,
    audiohole::State::Running, audiohole::State::Degraded, audiohole::State::Recovering,
    audiohole::State::Failed,  audiohole::State::Stopping};
const std::vector<audiohole::presentation::Message> events{
    audiohole::presentation::Message::Activated, audiohole::presentation::Message::Disconnected,
    audiohole::presentation::Message::ScanCompleted, audiohole::presentation::Message::Invalid,
    audiohole::presentation::Message::Reconnected};

class Viewer final : public wxFrame {
  public:
    Viewer(std::filesystem::path root, std::filesystem::path smoke)
        : wxFrame(nullptr, wxID_ANY, "AudioHole Personality Avatar Viewer", wxDefaultPosition,
                  wxSize(1080, 920)),
          root_(std::move(root)), smoke_(std::move(smoke)), timer_(this) {
        SetMinSize(FromDIP(wxSize(850, 650)));
        auto* outer = new wxBoxSizer(wxVERTICAL);
        auto* heading = new wxStaticText(
            this, wxID_ANY, wxString::FromUTF8("AudioHole · Personality Avatar Viewer"));
        auto font = heading->GetFont();
        font.SetPointSize(18);
        font.SetWeight(wxFONTWEIGHT_BOLD);
        heading->SetFont(font);
        outer->Add(heading, 0, wxALL, 12);
        outer->Add(new wxStaticText(
                       this, wxID_ANY,
                       wxString::FromUTF8("Independent asset viewer · Operational controls below "
                                          "are simulations; no audio engine is running.")),
                   0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
        auto* body = new wxBoxSizer(wxHORIZONTAL);
        auto* preview = new wxBoxSizer(wxVERTICAL);
        preview_sizer_ = preview;
        panel_ = new av::Panel(this);
        panel_->checkerboard(true);
        panel_->SetMinSize(FromDIP(wxSize(256, 384)));
        preview->Add(panel_, 1, wxEXPAND | wxALL, 8);
        status_ = new wxStaticText(this, wxID_ANY, wxString::FromUTF8("Loading approved assets…"));
        preview->Add(status_, 0, wxALL, 8);
        body->Add(preview, 1, wxEXPAND);
        auto* controls = new wxBoxSizer(wxVERTICAL);
        controls_parent_ = new wxScrolledWindow(this, wxID_ANY);
        controls_parent_->SetMinSize(FromDIP(wxSize(410, 100)));
        controls_parent_->SetScrollRate(0, FromDIP(12));
        personality_ = choice(controls, "Personality", {"Vanilla", "Norm", "Horu (UwU)"});
        style_ = choice(controls, "Visual variant", {"logo"});
        state_ = choice(controls, "Inspect state", states);
        auto* transport = new wxBoxSizer(wxHORIZONTAL);
        add_button(transport, "Play",
                   [this] { session_->player().pause(false, av::monotonic_ms()); });
        add_button(transport, "Pause",
                   [this] { session_->player().pause(true, av::monotonic_ms()); });
        add_button(transport, "Restart", [this] {
            inspect();
            session_->player().pause(false, av::monotonic_ms());
        });
        add_button(transport, "Step", [this] { session_->player().step(av::monotonic_ms()); });
        controls->Add(transport, 0, wxEXPAND | wxTOP, 8);
        controls->Add(new wxStaticText(controls_parent_, wxID_ANY, "Playback speed"), 0, wxTOP, 8);
        speed_ = new wxSpinCtrlDouble(controls_parent_, wxID_ANY, "1.0", wxDefaultPosition,
                                      wxDefaultSize, wxSP_ARROW_KEYS, .1, 4, 1, .1);
        speed_->SetDigits(1);
        speed_->SetName("Playback speed");
        controls->Add(speed_, 0, wxEXPAND);
        reduced_ = check(controls, "Reduced motion / static alternative");
        static_ = check(controls, "Inspect static expression");
        checker_ = check(controls, "Checkerboard background", true);
        metadata_ = check(controls, "Show metadata and diagnostics", true);
        size_ = choice(controls, "Display size",
                       {"Fit window", "128 × 192 compact", "144 × 216 GUI minimum", "256 × 384",
                        "512 × 768 native"});
        operation_ = choice(controls, "Simulated authoritative routing state",
                            {"Stopped", "Ready", "Starting", "Running (healthy / ambient)",
                             "Degraded", "Recovering", "Failed", "Stopping"});
        scanning_ = check(controls, "Simulated snapshot.scanning");
        event_ = choice(controls, "Simulated presentation event",
                        {"Activated", "Disconnected", "ScanCompleted", "Invalid", "Reconnected"});
        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        add_button(actions, "Send event", [this] { send_event(); });
        add_button(actions, "Choose asset root…", [this] { choose_root(); });
        controls->Add(actions, 0, wxTOP, 8);
        missing_ = check(controls, "Exercise missing asset-root fallback");
        details_ = new wxTextCtrl(controls_parent_, wxID_ANY, "", wxDefaultPosition,
                                  FromDIP(wxSize(390, 180)), wxTE_MULTILINE | wxTE_READONLY);
        details_->SetName("Animation metadata and diagnostics");
        controls->Add(details_, 1, wxEXPAND | wxTOP, 8);
        controls_parent_->SetSizer(controls);
        body->Add(controls_parent_, 0, wxEXPAND | wxALL, 12);
        outer->Add(body, 1, wxEXPAND);
        SetSizer(outer);
        Centre();
        personality_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { select_personality(); });
        style_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { select_pack(); });
        state_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { inspect(); });
        static_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { inspect(); });
        reduced_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
            session_->player().reduced_motion(reduced_->GetValue());
        });
        checker_->Bind(wxEVT_CHECKBOX,
                       [this](wxCommandEvent&) { panel_->checkerboard(checker_->GetValue()); });
        metadata_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
            details_->Show(metadata_->GetValue());
            controls_parent_->FitInside();
            Layout();
        });
        missing_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { create_session(); });
        operation_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { baseline(); });
        scanning_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { baseline(); });
        speed_->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent&) {
            session_->player().speed(speed_->GetValue(), av::monotonic_ms());
        });
        size_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { resize_preview(); });
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { update(); }, timer_.GetId());
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
            timer_.Stop();
            session_->shutdown();
            event.Skip();
        });
        create_session();
        timer_.Start(16);
        smoke_deadline_ = av::monotonic_ms() + 30000;
    }
    bool smoke_failed() const {
        return smoke_failed_;
    }

  private:
    std::filesystem::path root_, smoke_;
    std::unique_ptr<av::Session> session_;
    av::Panel* panel_{};
    wxStaticText* status_{};
    wxTextCtrl* details_{};
    wxChoice *personality_{}, *style_{}, *state_{}, *size_{}, *operation_{}, *event_{};
    wxCheckBox *reduced_{}, *static_{}, *checker_{}, *metadata_{}, *scanning_{}, *missing_{};
    wxSpinCtrlDouble* speed_{};
    wxTimer timer_;
    wxBoxSizer* preview_sizer_{};
    wxScrolledWindow* controls_parent_{};
    std::uint64_t revision_ = 1, event_sequence_ = 0;
    std::string last_details_;
    int smoke_step_ = 0;
    std::int64_t smoke_deadline_ = 0, smoke_wait_ = 0;
    bool smoke_failed_ = false;
    double maximum_present_ms_ = 0;
    Json smoke_results_ = Json::array();
    wxChoice* choice(wxSizer* sizer, const char* title, const std::vector<std::string>& values) {
        const auto label = wxString::FromUTF8(title);
        sizer->Add(new wxStaticText(controls_parent_, wxID_ANY, label), 0, wxTOP, 7);
        auto* control = new wxChoice(controls_parent_, wxID_ANY);
        control->SetName(label);
        for (const auto& value : values)
            control->Append(wxString::FromUTF8(value));
        control->SetSelection(0);
        sizer->Add(control, 0, wxEXPAND);
        return control;
    }
    wxCheckBox* check(wxSizer* sizer, const char* title, bool enabled = false) {
        auto* control = new wxCheckBox(controls_parent_, wxID_ANY, wxString::FromUTF8(title));
        control->SetValue(enabled);
        sizer->Add(control, 0, wxTOP, 7);
        return control;
    }
    template <class Function> void add_button(wxSizer* sizer, const char* title, Function action) {
        auto* control = new wxButton(controls_parent_, wxID_ANY, wxString::FromUTF8(title));
        sizer->Add(control, 0, wxRIGHT, 5);
        control->Bind(wxEVT_BUTTON, [action](wxCommandEvent&) { action(); });
    }
    std::string identity() const {
        return identities.at(personality_->GetSelection());
    }
    std::string style() const {
        return utf8(style_->GetStringSelection());
    }
    void create_session() {
        if (session_)
            session_->shutdown();
        session_ = std::make_unique<av::Session>(
            missing_->GetValue() ? root_ / "__missing_assets__" : root_, av::native_decoder());
        select_pack();
    }
    void select_personality() {
        style_->Clear();
        if (identity() == "vanilla")
            style_->Append("logo");
        else {
            style_->Append("chibi");
            style_->Append("regular");
        }
        style_->SetSelection(0);
        select_pack();
    }
    void select_pack() {
        session_->select(identity(), style(), av::monotonic_ms());
        revision_ = 1;
        session_->player().reduced_motion(reduced_->GetValue());
        session_->player().speed(speed_->GetValue(), av::monotonic_ms());
        baseline();
        panel_->present(session_->tick(av::monotonic_ms()));
    }
    void baseline() {
        const auto requested =
            bridge::baseline(operational.at(operation_->GetSelection()), scanning_->GetValue());
        session_->baseline(requested.state, ++revision_, requested.priority, av::monotonic_ms(),
                           true);
    }
    void inspect() {
        session_->inspect(states.at(state_->GetSelection()), static_->GetValue(),
                          av::monotonic_ms());
    }
    void send_event() {
        const auto request = bridge::reaction_for_snapshot(
            events.at(event_->GetSelection()), identity(),
            operational.at(operation_->GetSelection()), scanning_->GetValue());
        if (!request)
            return;
        const auto now = av::monotonic_ms();
        session_->react({request->state, "viewer:" + std::to_string(++event_sequence_), revision_,
                         now, 2000, request->priority},
                        now);
    }
    void choose_root() {
        wxDirDialog dialog(this, "Choose folder containing registry.json",
                           wxString::FromUTF8(root_.string()));
        if (dialog.ShowModal() == wxID_OK) {
            root_ = native_path(dialog.GetPath());
            create_session();
        }
    }
    void resize_preview() {
        const std::vector<wxSize> sizes{{256, 384}, {128, 192}, {144, 216}, {256, 384}, {512, 768}};
        auto size = sizes.at(size_->GetSelection());
        // Native means physical pixels, while the GUI minimum follows DIP.
        if (size_->GetSelection() == 2)
            size = FromDIP(size);
        panel_->SetMinSize(size);
        Layout();
        panel_->Refresh();
        panel_->SetMaxSize(size_->GetSelection() == 0 ? wxDefaultSize : size);
        auto* item = preview_sizer_->GetItem(panel_);
        item->SetProportion(size_->GetSelection() == 0 ? 1 : 0);
        item->SetFlag(size_->GetSelection() == 0 ? wxEXPAND | wxALL
                                                 : wxALIGN_CENTER_HORIZONTAL | wxALL);
        SetMinSize(wxSize(std::max(FromDIP(850), GetSizer()->GetMinSize().x), FromDIP(650)));
        Layout();
    }
    void update() {
        const auto now = av::monotonic_ms();
        auto frame = session_->tick(now);
        const auto paint_start = std::chrono::steady_clock::now();
        panel_->present(frame);
        maximum_present_ms_ =
            std::max(maximum_present_ms_, std::chrono::duration<double, std::milli>(
                                              std::chrono::steady_clock::now() - paint_start)
                                              .count());
        status_->SetLabel(wxString::FromUTF8(av::display_name(identity()) + " · " + frame.state +
                                             (frame.loading ? " · loading" : "") +
                                             (frame.image ? "" : " · hidden")));
        std::ostringstream text;
        text << "Personality: " << identity() << " / " << av::display_name(identity())
             << "\nStyle: " << style() << "\nBaseline: " << frame.baseline << " (revision "
             << revision_ << ")\nState: " << frame.state << " / " << frame.source
             << "\nFrame: " << frame.index + 1 << " / " << frame.frame_count << " · "
             << frame.exposure_ms << " ms\nPriority: " << frame.priority << " · "
             << (frame.paused ? "paused" : "playing")
             << "\nDecision: " << session_->last_decision().reason << "\nRoot: " << root_.string()
             << "\n";
        for (const auto& diagnostic : frame.diagnostics)
            text << diagnostic << "\n";
        if (text.str() != last_details_) {
            last_details_ = text.str();
            details_->ChangeValue(wxString::FromUTF8(last_details_));
        }
        if (!smoke_.empty())
            smoke(frame, now);
    }
    void smoke(const av::Presentation& frame, std::int64_t now) {
        if (now > smoke_deadline_) {
            finish_smoke(false, "viewer smoke timed out");
            return;
        }
        if (frame.loading || now < smoke_wait_)
            return;
        auto record = [&](const std::string& name, bool ok) {
            smoke_results_.push_back({{"check", name}, {"passed", ok}});
            if (!ok)
                smoke_failed_ = true;
        };
        switch (smoke_step_++) {
        case 0:
            record("Vanilla approved asset renders", frame.image && frame.personality == "vanilla");
            personality_->SetSelection(1);
            select_personality();
            break;
        case 1:
            record("Norm approved asset renders", frame.image && frame.personality == "default");
            personality_->SetSelection(2);
            select_personality();
            break;
        case 2:
            record("Horu approved asset renders", frame.image && frame.personality == "uwu");
            state_->SetStringSelection("talking");
            inspect();
            break;
        case 3:
            record("Talking uses production playback",
                   frame.image && frame.state == "talking" && frame.frame_count == 12);
            session_->player().pause(true, now);
            session_->player().step(now);
            break;
        case 4:
            record("Pause and frame step", frame.paused && frame.index == 1);
            reduced_->SetValue(true);
            session_->player().reduced_motion(true);
            break;
        case 5:
            record("Reduced motion selects static", frame.source == "reduced_motion_static");
            session_->player().pause(false, now);
            reduced_->SetValue(false);
            session_->player().reduced_motion(false);
            operation_->SetSelection(3);
            baseline();
            break;
        case 6:
            record("Healthy Running is ambient", frame.baseline == "idle");
            scanning_->SetValue(true);
            baseline();
            break;
        case 7:
            record("Scanning is working", frame.baseline == "working");
            operation_->SetSelection(6);
            baseline();
            break;
        case 8:
            record("Failure preempts cosmetic inspection",
                   frame.image && frame.baseline == "error" && frame.priority == 100);
            event_->SetSelection(0);
            send_event();
            smoke_wait_ = now + 250;
            break;
        case 9:
            record("Success cannot replace active failure", frame.state == "error");
            missing_->SetValue(true);
            create_session();
            break;
        case 10:
            record("Missing root hides without borrowing", !frame.image);
            missing_->SetValue(false);
            create_session();
            break;
        case 11:
            style_->SetStringSelection("regular");
            select_pack();
            break;
        case 12:
            record("Unavailable regular style hides", !frame.image);
            style_->SetStringSelection("chibi");
            operation_->SetSelection(3);
            scanning_->SetValue(false);
            select_pack();
            state_->SetStringSelection("talking");
            inspect();
            break;
        case 13:
            record("Restored Horu after faults", frame.image && frame.personality == "uwu");
            size_->SetSelection(1);
            resize_preview();
            break;
        case 14:
            record("Compact display is exactly 128x192",
                   panel_->GetClientSize() == wxSize(128, 192));
            size_->SetSelection(4);
            resize_preview();
            break;
        case 15:
            record("Native display is exactly 512x768",
                   panel_->GetClientSize() == wxSize(512, 768));
            size_->SetSelection(0);
            resize_preview();
            SetClientSize(FromDIP(wxSize(1280, 1000)));
            Layout();
            Centre();
            smoke_wait_ = now + 100;
            break;
        default: {
            record("Restored Horu after faults", frame.image && frame.personality == "uwu");
            record("Controls remain within the viewport",
                   controls_parent_->GetRect().GetRight() <= GetClientSize().x &&
                       controls_parent_->GetClientSize().x >= FromDIP(390));
            std::filesystem::create_directories(smoke_);
            Update();
            panel_->Update();
            wxClientDC source(this);
            wxBitmap bitmap(GetClientSize());
            wxMemoryDC target(bitmap);
            target.Blit(0, 0, bitmap.GetWidth(), bitmap.GetHeight(), &source, 0, 0);
            target.SelectObject(wxNullBitmap);
#ifdef _WIN32
            const auto screenshot = smoke_ / "viewer.bmp";
            const auto format =
                wxBITMAP_TYPE_BMP; // Parent wx build disables PNG; WIC is decode-only here.
#else
            const auto screenshot = smoke_ / "viewer.png";
            const auto format = wxBITMAP_TYPE_PNG;
#endif
            record("Viewer screenshot saved", bitmap.ConvertToImage().SaveFile(
                                                  wxString::FromUTF8(screenshot.string()), format));
            finish_smoke(!smoke_failed_, "complete");
            break;
        }
        }
    }
    void finish_smoke(bool success, const std::string& detail) {
        smoke_failed_ |= !success;
        std::filesystem::create_directories(smoke_);
        std::ofstream(smoke_ / "viewer-smoke.json")
            << Json{{"success", !smoke_failed_},
                    {"detail", detail},
                    {"maximum_present_ms", maximum_present_ms_},
                    {"bitmap_bytes", panel_->bitmap_bytes()},
                    {"decoded_bytes", session_->decoded_bytes()},
                    {"checks", smoke_results_}}
                   .dump(2)
            << '\n';
        timer_.Stop();
        session_->shutdown();
        Close(true);
    }
};
class Application final : public wxApp {
  public:
    bool OnInit() override {
        av::initialize_image_decoder();
        std::filesystem::path root = native_path(wxString::FromUTF8(AVATAR_DEFAULT_ROOT)), smoke;
        const auto executable = native_path(wxStandardPaths::Get().GetExecutablePath());
        for (const auto& candidate :
             {executable.parent_path(), executable.parent_path() / "assets/personalities",
              executable.parent_path() / "../.."}) {
            std::error_code error;
            if (std::filesystem::is_regular_file(candidate / "registry.json", error)) {
                root = std::filesystem::weakly_canonical(candidate);
                break;
            }
        }
        for (int i = 1; i < argc; ++i) {
            const std::string argument = utf8(argv[i]);
            if (argument == "--root" && i + 1 < argc)
                root = native_path(argv[++i]);
            else if (argument == "--smoke-out" && i + 1 < argc)
                smoke = native_path(argv[++i]);
        }
        frame_ = new Viewer(root, smoke);
        frame_->Show();
        return true;
    }

  private:
    Viewer* frame_{};
};
} // namespace
wxIMPLEMENT_APP(Application);
