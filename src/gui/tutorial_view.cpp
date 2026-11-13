#include "gui/controls.h"
#include "gui/frame.h"
#include "gui/shell_actions.h"
#include "model/first_run.h"
#include <cmath>
#include <sstream>
#include <wx/dcbuffer.h>
#include <wx/dcscreen.h>
#include <wx/generic/msgdlgg.h>
#include <wx/modalhook.h>
#include <wx/popupwin.h>
#include <wx/textdlg.h>
namespace audiohole::gui {
namespace {
void prepare_tutorial_overlay(wxWindow& window) {
    window.SetBackgroundStyle(wxBG_STYLE_PAINT);
    const auto handle = static_cast<HWND>(window.GetHandle());
    SetWindowLongPtrW(handle, GWL_EXSTYLE,
                      GetWindowLongPtrW(handle, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT |
                          WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
    SetLayeredWindowAttributes(handle, RGB(0, 0, 0), 255, LWA_COLORKEY);
}
// A non-activating, click-through outline. Only its pink stroke is opaque;
// native input goes to the real control below, including keyboard/accessibility.
class TutorialOutline final : public wxPopupWindow {
  public:
    explicit TutorialOutline(wxWindow* owner) : wxPopupWindow(owner, wxBORDER_NONE), pulse_(this) {
        prepare_tutorial_overlay(*this);
        Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
            wxAutoBufferedPaintDC dc(this);
            dc.SetBackground(*wxBLACK_BRUSH);
            dc.Clear();
            const int thickness = std::max(2, FromDIP(2));
            dc.SetPen(wxPen(palette::magenta(), thickness));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRoundedRectangle(GetClientRect().Deflate((thickness + 1) / 2), FromDIP(10));
        });
        Bind(wxEVT_TIMER, &TutorialOutline::on_pulse, this, pulse_.GetId());
        Bind(wxEVT_SHOW, &TutorialOutline::on_show, this);
    }
    ~TutorialOutline() override {
        // DestroyWindow can send WM_SHOWWINDOW from the base destructor, after
        // member destruction. Detach callbacks before the timer ceases to exist.
        pulse_.Stop();
        Unbind(wxEVT_TIMER, &TutorialOutline::on_pulse, this, pulse_.GetId());
        Unbind(wxEVT_SHOW, &TutorialOutline::on_show, this);
    }
    void place(const wxRect& bounds, bool animate) {
        bounds_ = bounds;
        if (animate && !pulse_.IsRunning()) {
            epoch_ = avatar::monotonic_ms();
            pulse_.Start(33);
        } else if (!animate)
            pulse_.Stop();
        update_bounds();
        Show();
    }

  private:
    wxTimer pulse_;
    wxRect bounds_;
    std::int64_t epoch_ = 0;
    void on_pulse(wxTimerEvent&) {
        update_bounds();
    }
    void on_show(wxShowEvent& event) {
        if (!event.IsShown())
            pulse_.Stop();
        event.Skip();
    }
    void update_bounds() {
        auto bounds = bounds_;
        if (pulse_.IsRunning()) {
            // One second out, one second back, one second resting. Only the
            // click-through popup moves; real controls and layout stay fixed.
            const auto phase = (avatar::monotonic_ms() - epoch_) % 3000;
            const double amount =
                phase < 2000 ? (1 - std::cos(phase * 3.141592653589793 / 1000)) / 2 : 0;
            bounds.Inflate(static_cast<int>(std::lround(FromDIP(3) * amount)));
        }
        if (GetScreenRect() != bounds) {
            SetSize(bounds);
            Refresh();
        }
    }
};
// An overlay inside the first dialogue frame, never a sizer item or input target.
class TutorialReplyArrow final : public wxPopupWindow {
  public:
    explicit TutorialReplyArrow(wxWindow* owner) : wxPopupWindow(owner, wxBORDER_NONE) {
        SetName("Tutorial reply guidance arrow");
        prepare_tutorial_overlay(*this);
        Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
            wxAutoBufferedPaintDC dc(this);
            dc.SetBackground(*wxBLACK_BRUSH);
            dc.Clear();
            dc.SetPen(wxPen(palette::magenta(), std::max(2, FromDIP(2))));
            const int left = FromDIP(6), right = GetClientSize().x - FromDIP(6);
            const int middle = GetClientSize().y / 2, tip = FromDIP(7);
            dc.DrawLine(left, middle, right, middle);
            dc.DrawLine(right - tip, middle - tip, right, middle);
            dc.DrawLine(right - tip, middle + tip, right, middle);
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
bool Frame::finish_tutorial(OnboardingOutcome outcome) {
    if (tutorial_marker_saved_ || smoke_)
        return true;
    const auto result = mark_first_run(first_run_marker_path(), outcome);
    if (!result.ok) {
        themed_message_box(
            text(result.detail + " The tutorial will appear again until completion can be saved."),
            "Tutorial completion", wxOK | wxICON_WARNING, this);
        return false;
    }
    tutorial_marker_saved_ = true;
    return true;
}
bool Frame::show_onboarding_greeting(OnboardingOutcome outcome) {
    if (tutorial_ || operational_warning_visible_ || !IsShown() || IsIconized())
        return false;
    const auto greeting = onboarding_greeting(outcome, personality_);
    if (!greeting)
        return false;
    technical_copy_.clear();
    commentary_copy_ = greeting->text;
    fallback_copy_.clear();
    sleep_copy_.clear();
    presentation::Copy copy;
    copy.personality_id = personality_;
    copy.expression = greeting->expression;
    avatar_host_->react(presentation::Message::Ready, copy, ++occurrence_, avatar::monotonic_ms());
    layout_header();
    Update();
    return true;
}
void Frame::skip_tutorial() {
    if (!tutorial_)
        return;
    const bool complete = tutorial_->complete();
    const wxString action = complete ? "End tutorial" : "Skip tutorial";
    if (!complete) {
        tutorial_->remind("B010");
        render_tutorial();
    }
    wxGenericMessageDialog dialog(this,
                                  complete ? "End the tutorial and restart AudioHole?"
                                           : "Skip the tutorial and restart AudioHole?",
                                  action, wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    dialog.SetYesNoLabels(action, "Cancel");
    dialog.SetEscapeId(wxID_NO);
    theme_dialog(&dialog);
    if (dialog.ShowModal() != wxID_YES) {
        tutorial_->dismiss_reminder();
        render_tutorial();
        return;
    }
    if (!finish_tutorial(complete ? OnboardingOutcome::Completed : OnboardingOutcome::Skipped))
        return;
    if (!complete)
        tutorial_->skip();
    if (restart_)
        restart_();
}
// GUI-thread tween in scroll units. A region is revealed once, so changing the
// source then destination in the same card does not restart or fight user input.
void Frame::scroll_tutorial_to(wxWindow& target, std::string_view region, int duration) {
    if (tutorial_scroll_region_ == region)
        return;
    tutorial_scroll_region_ = region;
    const auto start = profile_editor_->GetViewStart();
    int px = 0, py = 0;
    profile_editor_->GetScrollPixelsPerUnit(&px, &py);
    if (py <= 0)
        return;
    const auto point = profile_editor_->ScreenToClient(target.GetScreenPosition());
    const int client = profile_editor_->GetClientSize().y;
    const int maximum = std::max(0, (profile_editor_->GetVirtualSize().y - client + py - 1) / py);
    tutorial_scroll_from_ = start;
    tutorial_scroll_to_ =
        wxPoint(start.x,
                std::clamp(start.y + static_cast<int>(std::lround(
                                         (point.y + target.GetSize().y / 2.0 - client / 2.0) / py)),
                           0, maximum));
    BOOL animation = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animation, 0);
    if (preferences_->values().low_motion || animation == FALSE) {
        profile_editor_->Scroll(tutorial_scroll_to_);
        tutorial_scroll_duration_ = 0;
        return;
    }
    tutorial_scroll_started_ = avatar::monotonic_ms();
    tutorial_scroll_duration_ = duration;
}
bool Frame::advance_tutorial_scroll(std::int64_t now) {
    if (!tutorial_scroll_duration_)
        return false;
    if (!IsShown() || IsIconized() || notebook_->GetSelection() != 0) {
        tutorial_scroll_duration_ = 0;
        return false;
    }
    const double progress = std::clamp(
        static_cast<double>(now - tutorial_scroll_started_) / tutorial_scroll_duration_, 0.0, 1.0);
    const double eased = (1 - std::cos(progress * 3.141592653589793)) / 2;
    profile_editor_->Scroll(tutorial_scroll_from_.x,
                            tutorial_scroll_from_.y +
                                static_cast<int>(std::lround(
                                    (tutorial_scroll_to_.y - tutorial_scroll_from_.y) * eased)));
    if (progress >= 1)
        tutorial_scroll_duration_ = 0;
    return true;
}
void Frame::render_tutorial() {
    if (!tutorial_ || !notebook_)
        return;
    const auto view = tutorial_->view(device_id_, device_dirty_);
    if (tutorial_end_) {
        const wxString label = view.complete ? "End tutorial" : "Skip tutorial";
        if (tutorial_end_->GetLabel() != label) {
            tutorial_end_->SetLabel(label);
            tutorial_end_->SetToolTip(button_help(label));
        }
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
    const bool farewell = view.beat == "N036";
    if (farewell)
        choice_key += "end-tutorial";
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
            control->outline_colour = palette::magenta();
            control->UnsetToolTip();
            control->Bind(wxEVT_BUTTON, [this, id = choice.id](wxCommandEvent&) {
                if (tutorial_->choose(id))
                    render_tutorial();
            });
            tutorial_choices_layout_->Add(control, 0, wxEXPAND | wxALL, FromDIP(2));
            tutorial_choices_.push_back(control);
        }
        if (farewell) {
            auto* control = new ActionButton(tutorial_choices_panel_, "End tutorial");
            control->SetName("End tutorial");
            control->outline_colour = palette::magenta();
            control->UnsetToolTip();
            control->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { skip_tutorial(); });
            tutorial_choices_layout_->Add(control, 0, wxEXPAND | wxALL, FromDIP(2));
            tutorial_choices_.push_back(control);
        }
        // Choices are created after initial theme traversal. Apply the same
        // palette/native button policy each time the next pair is installed.
        apply_appearance();
        tutorial_choices_panel_->Layout();
        avatar_region_->Layout();
    }
    if (view.beat == "N001" && !tutorial_choices_.empty() && IsShown() && !IsIconized() &&
        comment_surface_->IsShownOnScreen()) {
        if (!tutorial_reply_arrow_)
            tutorial_reply_arrow_ = new TutorialReplyArrow(this);
        const auto card = comment_surface_->GetScreenRect();
        const auto button = tutorial_choices_.front()->GetScreenRect();
        const auto size = FromDIP(wxSize(48, 24));
        const int inset = FromDIP(16);
        const int y = std::clamp(button.y + button.height / 2 - size.y / 2, card.y + inset,
                                 card.GetBottom() - inset - size.y);
        tutorial_reply_arrow_->SetSize(wxRect(card.GetRight() - inset - size.x, y, size.x, size.y));
        tutorial_reply_arrow_->Show();
        tutorial_reply_arrow_->Refresh();
    } else if (tutorial_reply_arrow_)
        tutorial_reply_arrow_->Hide();
    BOOL reply_animation = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &reply_animation, 0);
    while (tutorial_reply_outlines_.size() < tutorial_choices_.size())
        tutorial_reply_outlines_.push_back(new TutorialOutline(this));
    for (std::size_t i = 0; i < tutorial_reply_outlines_.size(); ++i) {
        auto* outline = static_cast<TutorialOutline*>(tutorial_reply_outlines_[i]);
        if (farewell || i >= tutorial_choices_.size() || !IsShown() || IsIconized() ||
            !tutorial_choices_[i]->IsShownOnScreen()) {
            outline->Hide();
            continue;
        }
        auto* button = static_cast<ActionButton*>(tutorial_choices_[i]);
        if (button->outline_colour != button->GetBackgroundColour()) {
            button->outline_colour = button->GetBackgroundColour();
            button->Refresh();
        }
        outline->place(wxRect(tutorial_choices_[i]->GetScreenRect()).Inflate(FromDIP(1)),
                       !preferences_->values().low_motion && reply_animation != FALSE);
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
    int directed_row = -1;
    bool listbox_row = false;
    auto target = [&](const std::string& key) -> wxWindow* {
        if (key.starts_with("profile:") || key.starts_with("hole:")) {
            const bool profile = key.starts_with("profile:");
            const auto id = key.substr(profile ? 8 : 5);
            if (profile) {
                const auto found = std::find(profile_ids_.begin(), profile_ids_.end(), id);
                if (found != profile_ids_.end())
                    directed_row = static_cast<int>(found - profile_ids_.begin());
            } else {
                const auto found = std::find_if(draft_.holes.begin(), draft_.holes.end(),
                                                [&](const auto& hole) { return hole.id == id; });
                if (found != draft_.holes.end())
                    directed_row = static_cast<int>(found - draft_.holes.begin());
            }
            listbox_row = true;
            return directed_row >= 0 ? (profile ? profiles_ : holes_) : nullptr;
        }
        if (key.starts_with("device:")) {
            const auto id = key.substr(7);
            for (const auto& [list, ids] : {std::pair{capture_devices_, &capture_device_ids_},
                                            std::pair{playback_devices_, &playback_device_ids_}}) {
                const auto found = std::find(ids->begin(), ids->end(), id);
                if (found != ids->end()) {
                    directed_row = static_cast<int>(found - ids->begin());
                    return list;
                }
            }
            return nullptr;
        }
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
        if (key == "profile_list")
            return profiles_;
        if (key == "hole_list")
            return holes_;
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
    BOOL client_animation = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &client_animation, 0);
    const bool animate = !preferences_->values().low_motion && client_animation != FALSE;
    for (std::size_t i = 0; i < tutorial_highlights_.size(); ++i) {
        auto* highlight = static_cast<TutorialOutline*>(tutorial_highlights_[i]);
        directed_row = -1;
        listbox_row = false;
        auto* directed = i < view.targets.size() ? target(view.targets[i]) : nullptr;
        if (directed && step_changed && profile_editor_->IsDescendant(directed)) {
            const bool forward = view.beat == "N020" || view.beat == "N021";
            const bool reverse = view.beat == "N022" || view.beat == "N023";
            if (forward || reverse)
                scroll_tutorial_to(*(forward ? forward_.source : reverse_.source)->GetParent(),
                                   forward ? "forward" : "return", 3000);
            else if (view.beat == "N021A")
                scroll_tutorial_to(*directed, "exclusive", 1500);
            else
                scroll_tutorial_to(*directed, view.beat, 3000);
        }
        if (directed && !directed->IsShownOnScreen()) {
            for (std::size_t page = 0; page < notebook_->GetPageCount(); ++page)
                if (notebook_->GetPage(page)->IsDescendant(directed)) {
                    directed = navigation_[page];
                    directed_row = -1;
                    break;
                }
        }
        if (!directed || !IsShown() || IsIconized() || !directed->IsShownOnScreen()) {
            highlight->Hide();
            continue;
        }
        auto bounds = directed->GetScreenRect();
        if (directed_row >= 0 && listbox_row) {
            auto* list = static_cast<wxListBox*>(directed);
            if (step_changed)
                list->SetFirstItem(directed_row);
            RECT row{};
            if (SendMessageW(static_cast<HWND>(list->GetHandle()), LB_GETITEMRECT, directed_row,
                             reinterpret_cast<LPARAM>(&row)) == LB_ERR) {
                highlight->Hide();
                continue;
            }
            bounds = wxRect(list->ClientToScreen(wxPoint(row.left, row.top)),
                            wxSize(row.right - row.left, row.bottom - row.top));
            bounds.Intersect(wxRect(list->ClientToScreen(wxPoint(0, 0)), list->GetClientSize()));
        } else if (directed_row >= 0) {
            auto* list = static_cast<DeviceList*>(directed);
            const auto item = list->RowToItem(static_cast<unsigned>(directed_row));
            if (step_changed)
                list->EnsureVisible(item);
            bounds = list->GetItemRect(item);
            bounds.SetPosition(list->ClientToScreen(bounds.GetPosition()));
            bounds.Intersect(wxRect(list->GetMainWindow()->ClientToScreen(wxPoint(0, 0)),
                                    list->GetMainWindow()->GetClientSize()));
        }
        for (auto* parent = directed->GetParent(); parent && parent != this;
             parent = parent->GetParent())
            bounds.Intersect(
                wxRect(parent->ClientToScreen(wxPoint(0, 0)), parent->GetClientSize()));
        if (bounds.IsEmpty()) {
            highlight->Hide();
            continue;
        }
        bounds.Inflate(FromDIP(4));
        highlight->place(bounds, animate);
    }
}
} // namespace audiohole::gui

namespace audiohole::gui {
bool Frame::tutorial_smoke(const wxString& directory) {
    try {
        // Drive the controller clock explicitly. Rendering/artwork yields must
        // not add nondeterministic wall-clock ticks to the boundary assertions.
        timer_.Stop();
        render_tutorial();
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
            for (const auto* control : tutorial_choices_) {
                const auto* button = dynamic_cast<const ActionButton*>(control);
                require(button && button->outline_colour == (tutorial_->beat() == "N036"
                                                                 ? palette::magenta()
                                                                 : button->GetBackgroundColour()),
                        "Tutorial reply is missing its pink outline");
                require(control->GetToolTip() == nullptr,
                        "Tutorial reply must not have hover text");
            }
            for (std::size_t i = 0; i < tutorial_choices_.size(); ++i)
                require(tutorial_reply_outlines_.size() > i &&
                            tutorial_reply_outlines_[i]->IsShownOnScreen() ==
                                (tutorial_->beat() != "N036"),
                        "Tutorial reply pulse visibility differs from its current choices");
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
        auto rename = [&](bool is_profile, const wxString& name) {
            class ConfirmName final : public wxModalDialogHook {
              public:
                wxString name;
                bool asked = false;
                int Enter(wxDialog* dialog) override {
                    auto* edit = dynamic_cast<wxTextEntryDialog*>(dialog);
                    if (!edit)
                        return wxID_NONE;
                    asked = true;
                    edit->SetValue(name);
                    return wxID_OK;
                }
            } hook;
            hook.name = name;
            hook.Register();
            auto menu =
                is_profile ? profile_menu(viewed_) : hole_menu(draft_.holes.at(hole_index_).id);
            wxCommandEvent event(wxEVT_MENU, wxID_EDIT);
            menu->ProcessEvent(event);
            hook.Unregister();
            require(hook.asked, "Tutorial rename did not open the real context-menu name dialog");
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
        auto has_target = [&](const char* key) {
            const auto targets = tutorial_->view(device_id_, device_dirty_).targets;
            return std::find(targets.begin(), targets.end(), key) != targets.end();
        };
        auto listbox_highlight = [&](wxListBox* list, int row) {
            render_tutorial();
            require(tutorial_->view().targets.size() == 1,
                    "Rename step highlights more than the specific record");
            RECT native{};
            require(SendMessageW(static_cast<HWND>(list->GetHandle()), LB_GETITEMRECT, row,
                                 reinterpret_cast<LPARAM>(&native)) != LB_ERR,
                    "Rename highlight row has no native item rectangle");
            auto row_bounds =
                wxRect(list->ClientToScreen(wxPoint(native.left, native.top)),
                       wxSize(native.right - native.left, native.bottom - native.top));
            row_bounds.Intersect(
                wxRect(list->ClientToScreen(wxPoint(0, 0)), list->GetClientSize()));
            require(tutorial_highlights_[0]->IsShownOnScreen() &&
                        tutorial_highlights_[0]->GetScreenRect().Contains(row_bounds) &&
                        tutorial_highlights_[0]->GetSize().y <=
                            row_bounds.height + 2 * (FromDIP(4) + FromDIP(3)),
                    "Rename highlights the list instead of the requested row");
        };
        auto settle_scroll = [&](const char* region, int duration, wxWindow* target) {
            BOOL animation = TRUE;
            SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animation, 0);
            require(tutorial_scroll_region_ == region,
                    "Tutorial smooth scroll is targeting the wrong card");
            if (!preferences_->values().low_motion && animation != FALSE) {
                require(tutorial_scroll_duration_ == duration,
                        "Tutorial smooth scroll has the wrong duration");
                const auto from = tutorial_scroll_from_, to = tutorial_scroll_to_;
                advance_tutorial_scroll(tutorial_scroll_started_ + duration / 2);
                require(std::abs(profile_editor_->GetViewStart().y - (from.y + to.y) / 2) <= 1,
                        "Tutorial smooth scroll skipped its intermediate position");
                advance_tutorial_scroll(tutorial_scroll_started_ + duration);
                require(profile_editor_->GetViewStart() == to && !tutorial_scroll_duration_,
                        "Tutorial smooth scroll did not finish at its bounded destination");
            }
            render_tutorial();
            const auto point = profile_editor_->ScreenToClient(target->GetScreenPosition());
            const int center = point.y + target->GetSize().y / 2;
            const int view_center = profile_editor_->GetClientSize().y / 2;
            int px = 0, py = 0;
            profile_editor_->GetScrollPixelsPerUnit(&px, &py);
            const int position = profile_editor_->GetViewStart().y;
            const bool clamped =
                position == 0 || position * py + profile_editor_->GetClientSize().y >=
                                     profile_editor_->GetVirtualSize().y - py;
            require(clamped || std::abs(center - view_center) <= py,
                    "Tutorial configuration target was not centered in the editor");
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
        require(personality_ == "default" && !show_onboarding_greeting(OnboardingOutcome::Skipped),
                "Tutorial did not remain Norm-only or admitted a working greeting");
        require(
            tutorial_reply_arrow_ && tutorial_reply_arrow_->IsShownOnScreen() &&
                comment_surface_->GetScreenRect().Contains(tutorial_reply_arrow_->GetScreenRect()),
            "First interaction is missing its in-frame reply arrow");
        const auto initial_card = comment_surface_->GetScreenRect();
        const auto initial_avatar = agent_display_->GetScreenRect();
        for (int appearance : {0, 1, 2}) {
            theme_choice_->SetSelection(appearance);
            wxCommandEvent event(wxEVT_CHOICE, theme_choice_->GetId());
            theme_choice_->GetEventHandler()->ProcessEvent(event);
            check("N001");
            for (const auto* control : tutorial_choices_)
                require(control->GetBackgroundColour() ==
                                tutorial_choices_panel_->GetBackgroundColour() &&
                            control->GetForegroundColour() ==
                                tutorial_choices_panel_->GetForegroundColour(),
                        "Tutorial choice does not use the selected appearance palette");
        }
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
        {
            BOOL animation = TRUE;
            SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animation, 0);
            const auto bounds = tutorial_choices_[0]->GetScreenRect();
            if (!preferences_->values().low_motion && animation != FALSE) {
                int smallest = INT_MAX, largest = 0;
                const auto deadline = avatar::monotonic_ms() + 3100;
                while (avatar::monotonic_ms() < deadline) {
                    wxYield();
                    const auto width = tutorial_reply_outlines_[0]->GetSize().x;
                    smallest = std::min(smallest, width);
                    largest = std::max(largest, width);
                    require(tutorial_choices_[0]->GetScreenRect() == bounds,
                            "Reply pulse changed the actual button layout");
                    wxMilliSleep(10);
                }
                require(smallest == bounds.width + 2 * FromDIP(1) &&
                            largest == smallest + 2 * FromDIP(3),
                        "Tutorial response outline did not pulse with the guidance timing");
            }
            const auto original = preferences_->values();
            auto reduced = original;
            reduced.low_motion = true;
            require(preferences_->update(reduced).ok, "Cannot enable Low motion for replies");
            render_tutorial();
            require(tutorial_reply_outlines_[0]->GetScreenRect() ==
                        wxRect(bounds).Inflate(FromDIP(1)),
                    "Low motion did not leave tutorial replies statically outlined");
            require(preferences_->update(original).ok, "Cannot restore reply motion preferences");
            render_tutorial();
        }
        capture("first-reply");
        choice(1);
        check("N002");
        require(!tutorial_reply_arrow_->IsShown() &&
                    comment_surface_->GetScreenRect() == initial_card &&
                    agent_display_->GetScreenRect() == initial_avatar,
                "First reply arrow persists or changes the card/avatar layout");
        require(commentary_copy_.find("wobbly desk") != std::string::npos,
                "Tutorial choice selected the wrong complete variant");
        const auto image_deadline = avatar::monotonic_ms() + 8000;
        while ((!agent_display_->presentation().image ||
                agent_display_->presentation().state != tutorial_->view().expression ||
                !agent_display_->presentation().reaction) &&
               avatar::monotonic_ms() < image_deadline) {
            poll_avatar();
            wxYield();
            wxMilliSleep(5);
        }
        require(bool(agent_display_->presentation().image) &&
                    agent_display_->presentation().state == tutorial_->view().expression &&
                    agent_display_->presentation().reaction,
                "Tutorial Norm expression did not play for the current dialogue");
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
        BOOL client_animation = TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &client_animation, 0);
        if (!preferences_->values().low_motion && client_animation != FALSE) {
            const auto control_bounds = scan_start_->GetScreenRect();
            const auto base = wxRect(control_bounds).Inflate(FromDIP(4));
            int minimum = INT_MAX, maximum = 0;
            const auto deadline = avatar::monotonic_ms() + 3300;
            while (avatar::monotonic_ms() < deadline) {
                wxYield();
                const auto bounds = tutorial_highlights_[0]->GetScreenRect();
                minimum = std::min(minimum, bounds.width);
                maximum = std::max(maximum, bounds.width);
                require(scan_start_->GetScreenRect() == control_bounds,
                        "Tutorial pulse moved the actual control");
                wxMilliSleep(10);
            }
            require(minimum == base.width && maximum == base.width + 2 * FromDIP(3),
                    "Tutorial guidance did not expand and return by the expected few pixels");
        }
        const auto original_preferences = preferences_->values();
        auto reduced = original_preferences;
        reduced.low_motion = true;
        require(preferences_->update(reduced).ok, "Could not enable Low motion in smoke memory");
        render_tutorial();
        require(tutorial_highlights_[0]->GetScreenRect() ==
                    wxRect(scan_start_->GetScreenRect()).Inflate(FromDIP(4)),
                "Low motion did not restore the static guidance outline");
        require(preferences_->update(original_preferences).ok,
                "Could not restore smoke preferences");
        render_tutorial();
        {
            // Exercise teardown while still shown/animating. Native destruction
            // must not call the show handler after its timer member is gone.
            auto temporary = std::make_unique<TutorialOutline>(this);
            temporary->place(wxRect(scan_start_->GetScreenRect()).Inflate(FromDIP(4)), true);
        }
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
        capture("device-selection");
        auto row_highlight = [&](DeviceList* list, const std::vector<std::string>& ids,
                                 const char* id) {
            const auto targets = tutorial_->view(device_id_, device_dirty_).targets;
            const auto key = std::string("device:") + id;
            const auto target = std::find(targets.begin(), targets.end(), key);
            require(target != targets.end(), "Tutorial is missing the intended device-row target");
            const auto row = std::find(ids.begin(), ids.end(), id);
            require(row != ids.end(), "Guided device row no longer exists");
            auto* highlight =
                tutorial_highlights_.at(static_cast<std::size_t>(target - targets.begin()));
            const int expected_row = static_cast<int>(row - ids.begin());
            wxRect logical, visible, actual;
            int hit = wxNOT_FOUND;
            // Native column layout/scroll messages can be pending after a reply.
            // Pump only presentation, then sample a freshly positioned outline.
            const auto deadline = avatar::monotonic_ms() + 1000;
            do {
                wxYield();
                render_tutorial();
                logical = list->GetItemRect(list->RowToItem(expected_row));
                logical.SetPosition(list->ClientToScreen(logical.GetPosition()));
                visible = logical;
                visible.Intersect(wxRect(list->GetMainWindow()->ClientToScreen(wxPoint(0, 0)),
                                         list->GetMainWindow()->GetClientSize()));
                for (auto* parent = list->GetParent(); parent && parent != this;
                     parent = parent->GetParent())
                    visible.Intersect(
                        wxRect(parent->ClientToScreen(wxPoint(0, 0)), parent->GetClientSize()));
                actual = highlight->GetScreenRect();
                hit = list->HitTest(list->ScreenToClient(
                    wxPoint(actual.x + actual.width / 2, actual.y + actual.height / 2)));
                // Compare visible pixels, not the logical column width (which
                // can extend under clipping). Independently hit-test the row ID
                // and bound thickness, including separate rounded DIP margins.
                if (!visible.IsEmpty() && highlight->IsShownOnScreen() &&
                    actual.Contains(visible) && hit == expected_row &&
                    actual.height <= visible.height + 2 * (FromDIP(4) + FromDIP(3)))
                    return;
                wxMilliSleep(5);
            } while (avatar::monotonic_ms() < deadline);
            auto rectangle = [](const wxRect& r) {
                return std::to_string(r.x) + "," + std::to_string(r.y) + "," +
                       std::to_string(r.width) + "," + std::to_string(r.height);
            };
            std::ostringstream error;
            error << "Tutorial row outline mismatch: " << id << " beat=" << tutorial_->beat()
                  << " logical=" << rectangle(logical) << " visible=" << rectangle(visible)
                  << " outline=" << rectangle(actual) << " shown=" << highlight->IsShownOnScreen()
                  << " hit=" << hit << " expected_row=" << expected_row << " dpi=" << GetDPI().x;
            throw std::runtime_error(error.str());
        };
        row_highlight(capture_devices_, capture_device_ids_, "tutorial-mic");
        row_highlight(playback_devices_, playback_device_ids_, "tutorial-speakers");
        {
            // Deliberately extend logical columns beyond the viewport. The old
            // whole-row containment assertion rejects this valid clipped outline.
            auto* status = capture_devices_->GetColumn(0);
            auto* name = capture_devices_->GetColumn(1);
            const int status_width = status->GetWidth(), name_width = name->GetWidth();
            status->SetWidth(capture_devices_->GetMainWindow()->GetClientSize().x + FromDIP(80));
            row_highlight(capture_devices_, capture_device_ids_, "tutorial-mic");
            const auto row =
                std::find(capture_device_ids_.begin(), capture_device_ids_.end(), "tutorial-mic");
            require(capture_devices_
                            ->GetItemRect(capture_devices_->RowToItem(
                                static_cast<int>(row - capture_device_ids_.begin())))
                            .width >
                        capture_devices_->GetMainWindow()->GetClientSize().x + FromDIP(40),
                    "Clipped-row regression fixture did not exceed the visible viewport");
            status->SetWidth(status_width);
            name->SetWidth(name_width);
            row_highlight(capture_devices_, capture_device_ids_, "tutorial-mic");
        }
        require(!has_target("device_name") && !has_target("device_save"),
                "Device rename/save highlighted before an intended selection");
        select_list(capture_devices_, capture_device_ids_, "tutorial-mixer-in");
        require(!has_target("device_name") && !has_target("device_save"),
                "Wrong selected device received rename/save guidance");
        select_list(capture_devices_, capture_device_ids_, "tutorial-mic");
        require(has_target("device_name") && !has_target("device_save"),
                "Correct selected device is missing name guidance before editing");
        device_name_->SetValue("My headset microphone");
        check("N015");
        require(has_target("device_name") && has_target("device_save"),
                "Unsaved intended device is missing rename/save guidance");
        click(device_save_);
        check("N015");
        require(!has_target("device_name") && !has_target("device_save"),
                "Saved device retained rename/save guidance");
        row_highlight(playback_devices_, playback_device_ids_, "tutorial-speakers");
        select_list(playback_devices_, playback_device_ids_, "tutorial-speakers");
        device_name_->SetValue("My headset speakers");
        click(device_save_);
        check("N016");
        select_page(0);
        check("N017");
        click(profile_new_);
        check("N018A");
        require(tutorial_choices_.empty(), "Profile rename incorrectly has dialogue buttons");
        listbox_highlight(profiles_, profiles_->GetSelection());
        rename(true, text(draft_.display_name));
        check("N018A");
        rename(true, "  My tutorial profile  ");
        check("N018");
        click(hole_new_);
        check("N018B");
        listbox_highlight(holes_, holes_->GetSelection());
        rename(false, "  My tutorial connection  ");
        check("N019");
        duplex_->SetValue(true);
        wxCommandEvent return_channel(wxEVT_CHECKBOX, duplex_->GetId());
        duplex_->GetEventHandler()->ProcessEvent(return_channel);
        check("N020");
        settle_scroll("forward", 3000, forward_.source->GetParent());
        capture("forward-centered");
        choose_endpoint(forward_.source, forward_.sources, "tutorial-mixer-in");
        check("N020");
        require(commentary_copy_.find("Almost") != std::string::npos,
                "Wrong-endpoint reminder missing");
        choose_endpoint(forward_.source, forward_.sources, "tutorial-mic");
        check("N021");
        choose_endpoint(forward_.destination, forward_.destinations, "tutorial-mixer-out");
        check("N021A");
        settle_scroll("exclusive", 1500, forward_.exclusive_preferred);
        forward_.exclusive_preferred->SetValue(true);
        wxCommandEvent exclusive(wxEVT_CHECKBOX, forward_.exclusive_preferred->GetId());
        forward_.exclusive_preferred->GetEventHandler()->ProcessEvent(exclusive);
        check("N022");
        settle_scroll("return", 3000, reverse_.source->GetParent());
        capture("return-centered");
        require(commentary_copy_.find("Let me scroll down for you.") != std::string::npos,
                "Return scroll introduction is missing its explanation");
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
        controller_->advance(9999);
        refresh();
        check("N027");
        require(wxWindow::FindWindowByName("Pretend routing activation progress", this) == nullptr,
                "Tutorial activation still has a progress bar");
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
        require(wxWindow::FindWindowByLabel("Skip tutorial", this) != nullptr &&
                    wxWindow::FindWindowByLabel("Replay Tutorial", this) == nullptr,
                "Tutorial Settings did not replace Replay with Skip");
        capture("settings");
        choice();
        check("N035");
        require(tutorial_->complete(), "Tutorial did not mark completion after Settings");
        require(tutorial_end_ && tutorial_end_->GetLabel() == "End tutorial",
                "Completed tutorial did not relabel Skip to End tutorial");
        choice();
        check("N036");
        require(controller_->snapshot().operating_mode == OperatingMode::Tutorial &&
                    tutorial_choices_.size() == 1 &&
                    tutorial_choices_[0]->GetLabel() == "End tutorial" &&
                    commentary_copy_.find("young grasshopper") != std::string::npos,
                "Completion switched environment or omitted the definitive farewell/exit action");
        capture("farewell");
        class KeepExploring final : public wxModalDialogHook {
          public:
            bool asked = false;
            int Enter(wxDialog* dialog) override {
                if (dialog->GetTitle() != "End tutorial")
                    return wxID_NONE;
                asked = true;
                return wxID_NO;
            }
        } confirmation;
        confirmation.Register();
        click(tutorial_choices_[0]);
        confirmation.Unregister();
        require(confirmation.asked && !closing_ && IsShown() && tutorial_->complete(),
                "End tutorial did not offer its cancellable restart confirmation");
        check("N036");
        confirmation.Register();
        click(tutorial_end_);
        confirmation.Unregister();
        require(!closing_ && tutorial_->complete(), "Settings End tutorial ignored Cancel");
        class ConfirmRestart final : public wxModalDialogHook {
          public:
            wxString action;
            bool asked = false;
            int Enter(wxDialog* dialog) override {
                if (dialog->GetTitle() != action)
                    return wxID_NONE;
                auto* message = dynamic_cast<wxGenericMessageDialog*>(dialog);
                if (!message ||
                    message->GetMessage() != (action == "End tutorial"
                                                  ? "End the tutorial and restart AudioHole?"
                                                  : "Skip the tutorial and restart AudioHole?") ||
                    message->GetYesLabel() != action || message->GetNoLabel() != "Cancel")
                    throw std::runtime_error(
                        "Tutorial restart confirmation has incorrect copy/buttons");
                asked = true;
                return wxID_YES;
            }
        } confirm;
        const auto restart = restart_;
        bool restarted = false;
        restart_ = [&] { restarted = true; };
        confirm.action = "End tutorial";
        confirm.Register();
        click(tutorial_choices_[0]);
        confirm.Unregister();
        require(confirm.asked && restarted, "Norm End tutorial did not request a restart");
        auto completed_lesson = std::move(tutorial_);
        tutorial_ = std::make_unique<Tutorial>();
        restarted = false;
        confirm.action = "Skip tutorial";
        confirm.asked = false;
        confirm.Register();
        skip_tutorial();
        confirm.Unregister();
        require(confirm.asked && restarted, "Skip tutorial did not request a restart");
        tutorial_ = std::move(completed_lesson);
        restart_ = restart;
        require(std::filesystem::exists(marker) == marker_existed,
                "Tutorial smoke changed the installed completion flag");
        if (!directory.empty()) {
            wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
            std::ofstream out(std::filesystem::path(directory.ToStdWstring()) / "smoke-result.txt");
            out << "PASS: actual tutorial controls, authored branches, identity-based renames and "
                   "assignments, 20-second scan / 10-second activation completion events, "
                   "microphone acknowledgement, pulsing replies without tooltips, first-reply "
                   "arrow, "
                   "pulsing/clipped row guidance with Low motion, safe animated-popup teardown, "
                   "centered scroll tweens, skip/end restart confirmations, restricted Settings, "
                   "no installed flag writes. "
                   "DPI: "
                << GetDPI().x << ".\n";
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
