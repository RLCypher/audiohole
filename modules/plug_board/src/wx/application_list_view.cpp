#include "audiohole/plug_board/application_list.hpp"
#include "dialogs.hpp"
#include "native_theme.hpp"
#include "view_state.hpp"
#include <wx/wrapsizer.h>

namespace audiohole::plug_board::desktop {
namespace {
constexpr auto instructions =
    "Apps on this list will not be shown on the inactive list.\n"
    "If the app is on the list it will still be shown as active if it is producing audio.\n"
    "Type the names of apps you want to exempt, separated by commas.";
wxString exemption_text(const std::vector<std::string>& names) {
    wxString result;
    for (const auto& name : names) {
        if (!result.empty())
            result += ", ";
        result += ui_text(name);
    }
    return result;
}
} // namespace
class ExemptAppsDialog final : public wxDialog {
  public:
    explicit ExemptAppsDialog(PlugBoardView::Implementation& view)
        : wxDialog(&view.panel, wxID_ANY, "Exempt apps", wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          view_(view), timer_(this) {
        SetName("Exempt apps dialog");
        original_ = view.snapshot->library->inactive_exempt_apps;
        auto* layout = new wxBoxSizer(wxVERTICAL);
        auto* message = new wxStaticText(this, wxID_ANY, instructions);
        message->SetName("Exempt app instructions");
        message->Wrap(FromDIP(580));
        layout->Add(message, 0, wxEXPAND | wxALL, FromDIP(12));
        names_ = new wxTextCtrl(this, wxID_ANY, exemption_text(original_), wxDefaultPosition,
                                FromDIP(wxSize(580, -1)), wxTE_MULTILINE);
        names_->SetName("Exempt app names");
        names_->SetToolTip(
            "Enter app names separated by commas. Only inactive entries are hidden.");
        names_->SetMaxLength(65536);
        names_->SetMinSize(wxSize(FromDIP(380), names_->GetCharHeight() * 2 + FromDIP(12)));
        layout->Add(names_, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
        status_ = new wxStaticText(this, wxID_ANY, " ");
        status_->SetName("Exempt app save status");
        layout->Add(status_, 0, wxEXPAND | wxALL, FromDIP(12));
        auto* actions = new wxWrapSizer(wxHORIZONTAL);
        common_ = new Button(this, "Exempt common Windows processes",
                             "Append common process names. Review the list, then choose Apply.");
        common_->SetName("Exempt common Windows processes");
        apply_ = new Button(this, "Apply",
                            "Save this inactive-app filter. Routing assignments stay intact.",
                            wxID_APPLY);
        apply_->SetName("Apply app exemptions");
        auto* close = new Button(this, "Close", "Close this window. Unapplied edits are not saved.",
                                 wxID_CLOSE);
        close->SetName("Close app exemptions");
        for (auto* button : {common_, apply_, close})
            actions->Add(button, 0, wxALL, FromDIP(4));
        layout->Add(actions, 0, wxEXPAND | wxALL, FromDIP(8));
        SetSizer(layout);
        const auto minimum = layout->CalcMin();
        SetClientSize(
            wxSize(std::max(minimum.x, FromDIP(600)), minimum.y + names_->GetCharHeight() * 3));
        SetMinSize(GetSize() - wxSize(0, names_->GetCharHeight() * 3));
        Layout();
        common_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            try {
                auto names = parse_exemptions(names_->GetValue().ToStdString(wxConvUTF8));
                names.insert(names.end(), common_host_names().begin(), common_host_names().end());
                names_->ChangeValue(exemption_text(normalize_exemptions(names)));
                set_status("Suggestions added. Choose Apply to save.");
            } catch (const std::exception& error) {
                set_status(ui_text(error.what()));
            }
        });
        apply_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            try {
                auto names = parse_exemptions(names_->GetValue().ToStdString(wxConvUTF8));
                const auto current = view_.module->snapshot();
                if (current->library->inactive_exempt_apps != original_)
                    throw std::runtime_error(
                        "The exemption list changed elsewhere. Close and reopen this window.");
                pending_names_ = std::move(names);
                pending_.emplace(view_.module->submit(SetInactiveExemptions{pending_names_},
                                                      current->library->revision));
                apply_->Disable();
                common_->Disable();
                names_->Disable();
                set_status("Saving…");
                timer_.Start(50);
            } catch (const std::exception& error) {
                set_status(ui_text(error.what()));
            }
        });
        Bind(
            wxEVT_TIMER,
            [this](wxTimerEvent&) {
                if (!pending_ ||
                    pending_->wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                    return;
                const auto result = pending_->get();
                pending_.reset();
                timer_.Stop();
                if (result.ok)
                    original_ = pending_names_;
                set_status(result.ok ? wxString("Saved.") : ui_text(result.message));
                apply_->Enable();
                common_->Enable();
                names_->Enable();
            },
            timer_.GetId());
        close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CLOSE); });
        SetEscapeId(wxID_CLOSE);
        CentreOnParent();
    }

  private:
    void set_status(const wxString& message) {
        status_->SetLabel(message);
        status_->Wrap(names_->GetSize().x);
        Layout();
    }
    PlugBoardView::Implementation& view_;
    wxTextCtrl* names_{};
    wxStaticText* status_{};
    Button *common_{}, *apply_{};
    wxTimer timer_;
    std::vector<std::string> original_, pending_names_;
    std::optional<std::future<CommandResult>> pending_;
};
void PlugBoardView::Implementation::exempt_apps_dialog() {
    if (busy() || !snapshot || !snapshot->library)
        return;
    ++interaction_depth;
    ExemptAppsDialog dialog(*this);
    show_themed_dialog(dialog, appearance);
    --interaction_depth;
    if (!interaction_depth && deferred_snapshot)
        render(std::exchange(deferred_snapshot, {}));
}
void PlugBoardView::Implementation::clear_inactive(const std::vector<std::string>& candidates) {
    if (busy() || !snapshot)
        return;
    std::vector<std::string> ids;
    for (const auto& app : snapshot->applications)
        if (app.activity == Activity::Inactive && !app.hidden_inactive &&
            (candidates.empty() ||
             std::find(candidates.begin(), candidates.end(), app.id) != candidates.end()))
            ids.push_back(app.id);
    if (!ids.empty())
        submit(ClearInactiveApplications{std::move(ids)});
}
void PlugBoardView::Implementation::rename_app(const std::string& id) {
    if (busy() || !application(id))
        return;
    const auto context = snapshot;
    const auto initial = application(id)->display_name;
    if (const auto value = name("Rename app", initial))
        submit(RenameApplication{id, *value}, context);
}
void PlugBoardView::Implementation::group_menu(const std::vector<std::string>& members,
                                               wxWindow& owner, wxPoint point) {
    if (busy() || members.empty())
        return;
    wxMenu menu;
    const int id = wxWindow::NewControlId();
    auto* item = menu.Append(new MenuItem(&menu, id, "Clear group"));
    const bool inactive = std::any_of(members.begin(), members.end(), [&](const auto& member) {
        const auto* app = application(member);
        return app && app->activity == Activity::Inactive;
    });
    item->Enable(inactive);
    apply_menu_appearance(menu, appearance);
    ++interaction_depth;
    const auto selected = options.menu ? options.menu(menu, {"Clear group"})
                                       : (owner.GetPopupMenuSelectionFromUser(menu, point) == id
                                              ? std::optional<std::size_t>{0}
                                              : std::nullopt);
    --interaction_depth;
    if (selected == 0 && inactive)
        clear_inactive(members);
    if (!interaction_depth && deferred_snapshot)
        render(std::exchange(deferred_snapshot, {}));
}
} // namespace audiohole::plug_board::desktop
