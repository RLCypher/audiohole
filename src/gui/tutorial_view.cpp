#include "gui/controls.h"
#include "gui/frame.h"
#include "gui/shell_actions.h"
#include "model/first_run.h"
#include <wx/dcbuffer.h>
#include <wx/dcscreen.h>
#include <wx/popupwin.h>
namespace audiohole::gui {
namespace {
// A non-activating, click-through outline. Only its pink stroke is opaque;
// native input goes to the real control below, including keyboard/accessibility.
class TutorialOutline final : public wxPopupWindow {
  public:
    explicit TutorialOutline(wxWindow* owner) : wxPopupWindow(owner, wxBORDER_NONE) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        const auto handle = static_cast<HWND>(GetHandle());
        SetWindowLongPtrW(handle, GWL_EXSTYLE,
                          GetWindowLongPtrW(handle, GWL_EXSTYLE) | WS_EX_LAYERED |
                              WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
        SetLayeredWindowAttributes(handle, RGB(0, 0, 0), 255, LWA_COLORKEY);
        Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
            wxAutoBufferedPaintDC dc(this);
            dc.SetBackground(*wxBLACK_BRUSH);
            dc.Clear();
            const int thickness = std::max(2, FromDIP(2));
            dc.SetPen(wxPen(palette::magenta(), thickness));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRoundedRectangle(GetClientRect().Deflate((thickness + 1) / 2), FromDIP(10));
        });
    }
};
} // namespace
void Frame::tutorial_event(std::string_view action) {
    if (!tutorial_)
        return;
    capture_draft();
    tutorial_->event(action, controller_->snapshot(), draft_, hole_index_,
                     notebook_->GetSelection());
    render_tutorial();
}
bool Frame::finish_tutorial() {
    if (tutorial_marker_saved_ || smoke_)
        return true;
    const auto result = mark_first_run(first_run_marker_path());
    if (!result.ok) {
        themed_message_box(
            text(result.detail + " The tutorial will appear again until completion can be saved."),
            "Tutorial completion", wxOK | wxICON_WARNING, this);
        return false;
    }
    tutorial_marker_saved_ = true;
    return true;
}
void Frame::skip_tutorial() {
    if (!tutorial_)
        return;
    tutorial_->remind("B010");
    render_tutorial();
    if (themed_message_box("Skip the tutorial and close AudioHole?\nNext launch opens the working "
                           "environment. Fictional devices and profiles will be discarded.",
                           "Skip Tutorial?", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION,
                           this) != wxYES) {
        tutorial_->dismiss_reminder();
        render_tutorial();
        return;
    }
    tutorial_->skip();
    if (finish_tutorial())
        exit();
}
void Frame::render_tutorial() {
    if (!tutorial_ || !notebook_)
        return;
    const auto view = tutorial_->view();
    if (tutorial_routing_progress_) {
        const bool was_shown = tutorial_routing_progress_->IsShown();
        tutorial_routing_progress_->Show(view.beat == "N027");
        if (was_shown != tutorial_routing_progress_->IsShown())
            performance_panel_->GetParent()->Layout();
        if (view.beat == "N027")
            tutorial_routing_progress_->SetValue(
                std::clamp(controller_->snapshot().tutorial_activation_percent, 0, 100));
    }
    // Completion is the only tutorial persistence boundary. Retry only on a
    // subsequent explicit choice/exit, not on the GUI refresh timer.
    if (view.complete && !tutorial_completion_attempted_) {
        tutorial_completion_attempted_ = true;
        finish_tutorial();
    }
    std::string choice_key;
    for (const auto& c : view.choices)
        choice_key += c.id + "\n";
    if (choice_key != tutorial_choice_key_) {
        for (auto* choice : tutorial_choices_) {
            tutorial_choices_layout_->Detach(choice);
            choice->Destroy();
        }
        tutorial_choices_.clear();
        tutorial_choice_key_ = choice_key;
        for (const auto& choice : view.choices) {
            auto* control = new ActionButton(tutorial_choices_panel_, text(choice.label));
            control->SetName(text("Tutorial choice " + choice.id));
            control->SetToolTip("Choose this reply to Norm. Your reply selects the next complete "
                                "dialogue variant.");
            control->Bind(wxEVT_BUTTON, [this, id = choice.id](wxCommandEvent&) {
                if (tutorial_->choose(id))
                    render_tutorial();
            });
            tutorial_choices_layout_->Add(control, 0, wxEXPAND | wxALL, FromDIP(2));
            tutorial_choices_.push_back(control);
        }
        tutorial_choices_panel_->Layout();
        avatar_region_->Layout();
    }
    const auto signature = view.beat + "\n" + view.text + "\n" + view.expression;
    const bool changed = signature != tutorial_rendered_;
    const bool step_changed = !tutorial_rendered_.starts_with(view.beat + "\n");
    const bool interrupted =
        technical_copy_.size() || commentary_copy_ != view.text || operational_warning_visible_;
    operational_warning_visible_ = false;
    technical_copy_.clear();
    commentary_copy_ = view.text;
    fallback_copy_.clear();
    sleep_copy_.clear();
    if (changed) {
        tutorial_rendered_ = signature;
        operational_warning_visible_ = false;
        technical_copy_.clear();
        commentary_copy_ = view.text;
        fallback_copy_.clear();
        sleep_copy_.clear();
        personality_heading_->Hide();
        avatar_host_->tutorial_expression(view.expression, ++occurrence_, avatar::monotonic_ms());
        layout_header();
        // A new beat begins at the top, even if the previous paragraph was
        // scrolled. Repaint after the heading/viewport geometry has settled.
        commentary_panel_->Scroll(0, 0);
        commentary_panel_->Refresh();
        commentary_->Refresh();
    }
    if (!changed && interrupted)
        layout_header();
    while (tutorial_highlights_.size() < view.targets.size())
        tutorial_highlights_.push_back(new TutorialOutline(this));
    auto target = [&](const std::string& key) -> wxWindow* {
        if (key == "devices_tab")
            return navigation_[1];
        if (key == "scan_tab")
            return navigation_[2];
        if (key == "profiles_tab")
            return navigation_[0];
        if (key == "settings_tab")
            return navigation_[3];
        if (key == "scan_level")
            return scan_mode_;
        if (key == "scan_scope")
            return scan_scope_;
        if (key == "scan_start")
            return scan_start_;
        if (key == "capture_list")
            return capture_devices_;
        if (key == "playback_list")
            return playback_devices_;
        if (key == "device_name")
            return device_name_;
        if (key == "device_save")
            return device_save_;
        if (key == "profile_new")
            return profile_new_;
        if (key == "hole_new")
            return hole_new_;
        if (key == "return")
            return duplex_;
        if (key == "forward_source")
            return forward_.source;
        if (key == "forward_destination")
            return forward_.destination;
        if (key == "exclusive")
            return forward_.exclusive_preferred;
        if (key == "return_source")
            return reverse_.source;
        if (key == "return_destination")
            return reverse_.destination;
        if (key == "profile_save")
            return profile_save_;
        if (key == "start")
            return start_;
        if (key == "stop")
            return stop_;
        return nullptr;
    };
    for (std::size_t i = 0; i < tutorial_highlights_.size(); ++i) {
        auto* highlight = tutorial_highlights_[i];
        auto* directed = i < view.targets.size() ? target(view.targets[i]) : nullptr;
        if (directed && step_changed && profile_editor_->IsDescendant(directed)) {
            const auto point = profile_editor_->ScreenToClient(directed->GetScreenPosition());
            int x = 0, y = 0, px = 0, py = 0;
            profile_editor_->GetViewStart(&x, &y);
            profile_editor_->GetScrollPixelsPerUnit(&px, &py);
            int reveal_x = x;
            if (px > 0 && point.x < 0)
                reveal_x = std::max(0, x + point.x / px - 1);
            else if (px > 0 && point.x + directed->GetSize().x > profile_editor_->GetClientSize().x)
                reveal_x =
                    x +
                    (point.x + directed->GetSize().x - profile_editor_->GetClientSize().x) / px + 1;
            if (py > 0)
                profile_editor_->Scroll(reveal_x, std::max(0, y + point.y / py - 1));
        }
        if (directed && !directed->IsShownOnScreen()) {
            for (std::size_t page = 0; page < notebook_->GetPageCount(); ++page)
                if (notebook_->GetPage(page)->IsDescendant(directed)) {
                    directed = navigation_[page];
                    break;
                }
        }
        if (!directed || !IsShown() || IsIconized() || !directed->IsShownOnScreen()) {
            highlight->Hide();
            continue;
        }
        auto bounds = directed->GetScreenRect();
        for (auto* parent = directed->GetParent(); parent && parent != this;
             parent = parent->GetParent())
            bounds.Intersect(
                wxRect(parent->ClientToScreen(wxPoint(0, 0)), parent->GetClientSize()));
        if (bounds.IsEmpty()) {
            highlight->Hide();
            continue;
        }
        bounds.Inflate(FromDIP(4));
        if (highlight->GetScreenRect() != bounds)
            highlight->SetSize(bounds);
        highlight->Show();
        highlight->Refresh();
    }
}
} // namespace audiohole::gui

namespace audiohole::gui {
bool Frame::tutorial_smoke(const wxString& directory) {
    try {
        // Drive the controller clock explicitly. Rendering/artwork yields must
        // not add nondeterministic wall-clock ticks to the boundary assertions.
        timer_.Stop();
        auto require = [](bool ok, const char* message) {
            if (!ok)
                throw std::runtime_error(message);
        };
        auto check = [&](const char* beat) {
            if (!tutorial_ || tutorial_->beat() != beat)
                throw std::runtime_error("Tutorial expected " + std::string(beat) + ", reached " +
                                         (tutorial_ ? tutorial_->beat() : "no tutorial"));
            require(commentary_copy_ == tutorial_->view().text && technical_copy_.empty(),
                    "Ordinary status displaced tutorial dialogue");
            require(!personality_heading_->IsShown(), "Tutorial rendered a speaker prefix/heading");
        };
        auto click = [&](wxButton* control) {
            require(control != nullptr && control->IsEnabled(),
                    "Tutorial action control missing or disabled");
            wxCommandEvent event(wxEVT_BUTTON, control->GetId());
            control->GetEventHandler()->ProcessEvent(event);
        };
        auto choice = [&](int index = 0) {
            require(index < static_cast<int>(tutorial_choices_.size()), "Tutorial choices missing");
            click(tutorial_choices_[index]);
        };
        auto choose_endpoint = [&](wxComboBox* control, const std::vector<std::string>& ids,
                                   const char* id) {
            const auto found = std::find(ids.begin(), ids.end(), id);
            if (found == ids.end())
                throw std::runtime_error("Tutorial endpoint not offered: " + std::string(id) +
                                         " at " + tutorial_->beat());
            control->SetSelection(static_cast<int>(found - ids.begin()));
            wxCommandEvent event(wxEVT_COMBOBOX, control->GetId());
            control->GetEventHandler()->ProcessEvent(event);
        };
        auto select_list = [&](DeviceList* list, const std::vector<std::string>& ids,
                               const char* id) {
            const auto found = std::find(ids.begin(), ids.end(), id);
            require(found != ids.end(), "Tutorial device list failed to refresh after discovery");
            list->SetSelection(static_cast<int>(found - ids.begin()));
            wxDataViewEvent selected(wxEVT_DATAVIEW_SELECTION_CHANGED, list,
                                     list->RowToItem(static_cast<unsigned>(found - ids.begin())));
            selected.SetEventObject(list);
            list->GetEventHandler()->ProcessEvent(selected);
            require(device_id_ == id, "Tutorial device-list selection did not select its record");
        };
        auto capture = [&](const char* name) {
            if (directory.empty())
                return;
            wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            Layout();
            Update();
            wxYield();
            commentary_panel_->Update();
            commentary_->Update();
            RedrawWindow(static_cast<HWND>(GetHandle()), nullptr, nullptr,
                         RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
            GdiFlush();
            const auto bounds = commentary_->GetScreenRect();
            const auto viewport = commentary_panel_->GetScreenRect();
            if (!commentary_->IsShownOnScreen() || bounds.width <= 0 || bounds.height <= 0 ||
                !bounds.Intersects(viewport))
                throw std::runtime_error(
                    wxString::Format("Tutorial narration is not visible: label=%d,%d,%d,%d "
                                     "viewport=%d,%d,%d,%d shown=%d chars=%zu",
                                     bounds.x, bounds.y, bounds.width, bounds.height, viewport.x,
                                     viewport.y, viewport.width, viewport.height,
                                     commentary_->IsShownOnScreen(), commentary_->GetLabel().size())
                        .ToStdString());
            wxBitmap bitmap(GetClientSize());
            wxMemoryDC destination(bitmap);
            wxScreenDC source;
            const auto origin = ClientToScreen(wxPoint(0, 0));
            destination.Blit(0, 0, GetClientSize().x, GetClientSize().y, &source, origin.x,
                             origin.y);
            destination.SelectObject(wxNullBitmap);
            // Desktop capture is review evidence, not a visual correctness assertion.
            bitmap.SaveFile(directory + "/" + wxString::FromUTF8(name) + ".bmp", wxBITMAP_TYPE_BMP);
        };
        check("N001");
        require(controller_->snapshot().endpoints.empty() &&
                    controller_->snapshot().profiles.empty(),
                "Tutorial leaked normal fixtures or saved records");
        const auto marker = first_run_marker_path();
        const bool marker_existed = std::filesystem::exists(marker);
        const auto initial = commentary_copy_;
        click(start_);
        require(!controller_->snapshot().armed,
                "Tutorial started routing before the directed action");
        tutorial_->dismiss_reminder();
        render_tutorial();
        require(commentary_copy_ == initial, "Early-start reminder lost the current dialogue");
        choice(1);
        check("N002");
        require(commentary_copy_.find("wobbly desk") != std::string::npos,
                "Tutorial choice selected the wrong complete variant");
        const auto image_deadline = avatar::monotonic_ms() + 8000;
        while (!agent_display_->presentation().image && avatar::monotonic_ms() < image_deadline) {
            poll_avatar();
            wxYield();
            wxMilliSleep(5);
        }
        require(bool(agent_display_->presentation().image), "Tutorial Norm artwork did not load");
        capture("introduction");
        choice();
        choice();
        check("N004");
        select_page(1);
        check("N005");
        select_page(2);
        check("N006");
        choice();
        choice();
        check("N007");
        scan_mode_->SetSelection(1);
        wxCommandEvent level(wxEVT_CHOICE, scan_mode_->GetId());
        scan_mode_->GetEventHandler()->ProcessEvent(level);
        scan_scope_->SetSelection(1);
        wxCommandEvent scope(wxEVT_CHOICE, scan_scope_->GetId());
        scan_scope_->GetEventHandler()->ProcessEvent(scope);
        check("N008");
        require(!tutorial_highlights_.empty() && tutorial_highlights_[0]->IsShownOnScreen(),
                "Tutorial did not highlight the directed scan button");
        const auto outline_style =
            GetWindowLongPtrW(static_cast<HWND>(tutorial_highlights_[0]->GetHandle()), GWL_EXSTYLE);
        require((outline_style & (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE)) ==
                    (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE),
                "Tutorial highlight can intercept input or activate itself");
        click(scan_start_);
        check("N009");
        capture("scanning");
        controller_->advance(19999);
        refresh();
        check("N009");
        require(controller_->snapshot().endpoints.empty(),
                "Tutorial discovered devices before scan completion");
        controller_->advance(1);
        refresh();
        check("N010");
        require(controller_->snapshot().endpoints.size() == 4,
                "Tutorial did not discover exactly four endpoints");
        choice();
        check("N011");
        select_page(1);
        check("N012");
        choice();
        choice();
        check("N014");
        require(capture_devices_->GetCount() == 2 && playback_devices_->GetCount() == 2,
                "Tutorial lists do not show two capture and two playback endpoints");
        select_list(capture_devices_, capture_device_ids_, "tutorial-mic");
        device_name_->SetValue("My headset microphone");
        check("N015");
        click(device_save_);
        check("N015");
        select_list(playback_devices_, playback_device_ids_, "tutorial-speakers");
        device_name_->SetValue("My headset speakers");
        click(device_save_);
        check("N016");
        select_page(0);
        check("N017");
        click(profile_new_);
        check("N018");
        click(hole_new_);
        check("N019");
        duplex_->SetValue(true);
        wxCommandEvent return_channel(wxEVT_CHECKBOX, duplex_->GetId());
        duplex_->GetEventHandler()->ProcessEvent(return_channel);
        check("N020");
        choose_endpoint(forward_.source, forward_.sources, "tutorial-mixer-in");
        check("N020");
        require(commentary_copy_.find("Almost") != std::string::npos,
                "Wrong-endpoint reminder missing");
        choose_endpoint(forward_.source, forward_.sources, "tutorial-mic");
        check("N021");
        choose_endpoint(forward_.destination, forward_.destinations, "tutorial-mixer-out");
        check("N021A");
        forward_.exclusive_preferred->SetValue(true);
        wxCommandEvent exclusive(wxEVT_CHECKBOX, forward_.exclusive_preferred->GetId());
        forward_.exclusive_preferred->GetEventHandler()->ProcessEvent(exclusive);
        check("N022");
        require(!reverse_.exclusive_preferred->GetValue(),
                "Tutorial changed Return exclusive preference");
        choose_endpoint(reverse_.source, reverse_.sources, "tutorial-mixer-in");
        check("N023");
        choose_endpoint(reverse_.destination, reverse_.destinations, "tutorial-speakers");
        check("N024");
        click(profile_save_);
        check("N025");
        capture("saved-profile");
        choice();
        check("N026");
        click(start_);
        check("N027");
        controller_->advance(19999);
        refresh();
        check("N027");
        require(tutorial_routing_progress_->IsShown(), "Tutorial activation progress is missing");
        controller_->advance(1);
        refresh();
        check("N027B");
        choice();
        check("N027A");
        choice();
        check("N028");
        require(tutorial_choices_.size() == 2 &&
                    tutorial_choices_[0]->GetLabel() == "I said something" &&
                    tutorial_choices_[1]->GetLabel() == "Testing 1, 2",
                "Microphone acknowledgement choices differ from the script");
        require(!controller_->snapshot().monitoring && !controller_->snapshot().input_peak,
                "Tutorial microphone joke opened an input check");
        bother();
        require(tutorial_->beat() == "N028", "Clicking Norm advanced the tutorial");
        choice(1);
        check("N030-A");
        choice();
        check("N031");
        choice();
        check("N032");
        choice();
        check("N033");
        select_page(3);
        check("N034");
        require(!personality_choice_->IsEnabled() && !animation_loops_->IsEnabled() &&
                    !dfa_slider_->IsEnabled(),
                "Tutorial personality/animation/DFA controls remain interactive");
        require(wxWindow::FindWindowByLabel("Skip Tutorial", this) != nullptr &&
                    wxWindow::FindWindowByLabel("Replay Tutorial", this) == nullptr,
                "Tutorial Settings did not replace Replay with Skip");
        capture("settings");
        choice();
        check("N035");
        require(tutorial_->complete(), "Tutorial did not mark completion after Settings");
        choice();
        check("N036");
        require(controller_->snapshot().operating_mode == OperatingMode::Tutorial &&
                    tutorial_choices_.empty(),
                "Completion switched environment or retained choices");
        require(std::filesystem::exists(marker) == marker_existed,
                "Tutorial smoke changed the installed completion flag");
        if (!directory.empty()) {
            wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            std::ofstream out(std::filesystem::path(directory.ToStdWstring()) / "smoke-result.txt");
            out << "PASS: actual tutorial controls, authored branches, identity-based renames and "
                   "assignments, 20-second operation completion events, microphone "
                   "acknowledgement, restricted Settings, no installed flag writes.\n";
            require(bool(out), "Could not write tutorial smoke evidence");
        }
        return true;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GUI tutorial smoke FAIL: %s\n", error.what());
        std::fflush(stderr);
        if (!directory.empty()) {
            wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            std::ofstream(std::filesystem::path(directory.ToStdWstring()) / "smoke-result.txt")
                << "FAIL: " << error.what() << '\n';
        }
        return false;
    }
}
} // namespace audiohole::gui
