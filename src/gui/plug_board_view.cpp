#include "audiohole/plug_board/windows.hpp"
#include "gui/controls.h"
#include "gui/frame.h"
#include <utility>
#include <wx/stdpaths.h>
#include <wx/utils.h>

namespace audiohole::gui {
namespace pb = audiohole::plug_board;
namespace {
const pb::Socket* socket(const pb::RoutingBoard& board, const std::string& id) {
    const auto found = std::find_if(board.working.sockets.begin(), board.working.sockets.end(),
                                    [&](const auto& value) { return value.id == id; });
    return found == board.working.sockets.end() ? nullptr : &*found;
}
// One authored response per successful edit, independent of worker publications,
// process discovery, policy retries and observed stream-location changes.
std::optional<presentation::Message> edit_message(const pb::ModuleSnapshot& before,
                                                  const pb::ModuleSnapshot& after) {
    using M = presentation::Message;
    if (!before.library || !after.library || before.library->revision == after.library->revision)
        return {};
    const auto* old = pb::find_board(*before.library, after.selected_board_id);
    const auto* now = pb::find_board(*after.library, after.selected_board_id);
    if (!old || !now)
        return {};
    if (old->working.sockets.size() > now->working.sockets.size())
        return M::PlugBoardSocketDeleted;
    if (old->working.sockets.size() < now->working.sockets.size())
        return M::PlugBoardSocketCreated;
    if (before.library->profile_associations != after.library->profile_associations)
        return M::PlugBoardBoardProfileAssociated;
    for (std::size_t index = 0; index < now->working.sockets.size(); ++index) {
        const auto& current = now->working.sockets[index];
        const auto* previous = socket(*old, current.id);
        if (!previous)
            continue;
        if (previous->display_name != current.display_name)
            return M::PlugBoardSocketRenamed;
        if (previous->endpoint.logical_device_id != current.endpoint.logical_device_id)
            return M::PlugBoardSocketEndpointChanged;
        if (previous->appearance.icon != current.appearance.icon)
            return M::PlugBoardSocketIconAssigned;
        if (previous->appearance.color != current.appearance.color)
            return M::PlugBoardSocketColorAssigned;
        if (old->working.sockets[index].id != current.id)
            return M::PlugBoardSocketMoved;
    }
    if (old->working.application_assignments != now->working.application_assignments)
        return M::PlugBoardAppMoved;
    return {};
}
// The fictional tutorial has no actual app streams to recreate after a policy
// change. Reflect its committed assignments in the fake observation stream.
// Normal Windows observations remain exclusively owned by the native backend.
void reflect_practice_playback(pb::Simulation& simulation, const pb::ModuleSnapshot& state,
                               const pb::RoutingBoard& board) {
    pb::SessionSnapshot sessions;
    bool changed = false;
    for (const auto& app : state.applications) {
        if (!app.observation)
            continue;
        const auto* target = socket(board, pb::assigned_socket(board.working, app.id));
        auto destination = state.inventory.default_windows_endpoints[0];
        if (target && target->endpoint.logical_device_id) {
            const auto endpoint =
                std::find_if(state.inventory.endpoints.begin(), state.inventory.endpoints.end(),
                             [&](const auto& value) {
                                 return value.logical_id == *target->endpoint.logical_device_id &&
                                        value.available && !value.ambiguous;
                             });
            destination = endpoint == state.inventory.endpoints.end()
                              ? std::nullopt
                              : std::optional<std::string>(endpoint->windows_id);
        }
        for (auto session : app.observation->sessions) {
            if (destination && !destination->empty() && session.endpoint_id != *destination) {
                session.endpoint_id = *destination;
                changed = true;
            }
            sessions.sessions.push_back(std::move(session));
        }
    }
    if (changed)
        simulation.set_sessions(std::move(sessions));
}
} // namespace

void Frame::prepare_plug_tutorial_apps() {
    if (!tutorial_)
        for (const auto* device : configuration_devices(controller_->snapshot()))
            if (!device->capture && device->availability == Availability::Available) {
                plug_simulated_default_ = device->id;
                break;
            }
    pb::SessionSnapshot sessions;
    std::vector<pb::ProcessEvidence> processes;
    std::map<std::string, pb::ApplicationImage> images;
    const auto assets =
        std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring())
            .parent_path() /
        "assets" / "tutorial" / "apps";
    unsigned pid = 100;
    for (const auto& [name, icon] : std::vector<std::pair<std::string, std::string>>{
             {"Spooty", "Spooty.png"},
             {"Felony Jaywalking LXVII", "Felony Jaywalking.png"},
             {"Dissonance", "Dissonance.png"},
             {"Internet Exploder", "Internet Exploder.png"},
             {"Bar Simulator", "Bar simulator.png"},
             {"Epic Fail Launcher", "Epic fail launcher.png"}}) {
        pb::SessionObservation session;
        session.endpoint_id = plug_simulated_default_;
        session.instance_id = "tutorial:" + name;
        session.display_name = name;
        session.identity = {pb::IdentityKind::Win32Image, "C:\\Tutorial\\" + name + ".exe"};
        session.process = pb::ProcessToken{++pid, 100000 + pid, session.identity.value};
        session.state = pb::SessionState::Active;
        processes.push_back({session.identity, *session.process});
        auto preview = package_preview(assets / icon);
        if (preview.IsOk()) {
            preview = preview.Scale(64, 64, wxIMAGE_QUALITY_HIGH);
            pb::ApplicationImage image;
            image.width = image.height = 64;
            image.application_specific = true;
            image.source = "Owner-supplied fictional tutorial application icon";
            image.result = {true, "tutorial_icon", "Fictional application artwork."};
            image.rgba.reserve(64 * 64 * 4);
            for (unsigned pixel = 0; pixel < 64 * 64; ++pixel) {
                const auto* rgb = preview.GetData() + pixel * 3;
                image.rgba.insert(image.rgba.end(),
                                  {rgb[0], rgb[1], rgb[2],
                                   static_cast<std::uint8_t>(
                                       preview.HasAlpha() ? preview.GetAlpha()[pixel] : 255)});
            }
            images.emplace(session.identity.value, std::move(image));
        }
        sessions.sessions.push_back(std::move(session));
    }
    plug_simulation_->set_processes(std::move(processes));
    plug_simulation_->set_images(std::move(images));
    plug_simulation_->set_sessions(std::move(sessions));
}

void Frame::build_plug_board() {
    pb::ModuleOptions options;
    if (!tutorial_ && !smoke_) {
        const auto root = preferences_->data_root(controller_->snapshot().operating_mode);
        options.data_root = root / "plug-board";
        options.local_root = root.parent_path() / "test-local" / "plug-board";
    }
    options.icon_pixels = static_cast<unsigned>(FromDIP(32));
    if (tutorial_ || smoke_) {
        options.memory_only = true;
        plug_simulation_ = std::make_shared<pb::Simulation>();
        prepare_plug_tutorial_apps();
        options.backend = [simulation = plug_simulation_] {
            return pb::make_simulated_backend(simulation);
        };
    } else {
        options.resolve_system_defaults = true;
        // Notification registration observes System default-role changes even
        // while hidden; standalone enumeration remains explicitly disabled.
        options.backend = [] { return pb::make_windows_backend(true); };
    }
    plug_board_ = std::make_unique<pb::Module>(std::move(options));
    pb::desktop::ViewOptions view;
    if (smoke_)
        view.menu = [this](const wxMenu&,
                           const std::vector<std::string>& choices) -> std::optional<std::size_t> {
            const auto wanted = std::exchange(plug_smoke_menu_, {});
            if (!wanted)
                return {};
            const auto found = std::find(choices.begin(), choices.end(), *wanted);
            if (found == choices.end())
                throw std::runtime_error("Plug Board did not construct the expected menu action: " +
                                         *wanted);
            return static_cast<std::size_t>(found - choices.begin());
        };
    view.confirm = [this](const std::string& title, const std::string& message) {
        return themed_message_box(text(message), text(title),
                                  wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) == wxYES;
    };
    view.completed = [this](const pb::CommandResult& result) {
        if (!closing_ && !result.ok) {
            if (tutorial_) {
                tutorial_->repeat();
                render_tutorial();
                if (!smoke_)
                    themed_message_box(text(result.message), "Plug Board",
                                       wxOK | wxICON_INFORMATION, this);
            } else
                show_result({false, result.message});
        }
        refresh_plug_board();
    };
    view.open_url = [this](const std::string& url) {
        // The tutorial never launches Windows Settings or modifies real policy.
        if (tutorial_) {
            tutorial_->remind("PB-R05");
            render_tutorial();
            return true;
        }
        if (smoke_)
            return true;
        const bool opened = wxLaunchDefaultBrowser(text(url));
        if (opened)
            present(presentation::Message::PlugBoardWindowsMixerOpened);
        return opened;
    };
    plug_board_view_ = new pb::desktop::PlugBoardView(notebook_, *plug_board_, std::move(view));
    notebook_->AddPage(plug_board_view_, "Plug Board");
}

void Frame::theme_plug_board(bool contrast) {
    if (!plug_board_view_)
        return;
    pb::desktop::ViewAppearance appearance;
    appearance.dark = dark_appearance_;
    appearance.high_contrast = contrast;
    appearance.surface = GetBackgroundColour();
    appearance.input = dark_appearance_ ? palette::input() : *wxWHITE;
    appearance.foreground = GetForegroundColour();
    appearance.muted = dark_appearance_ ? wxColour(151, 166, 182) : wxColour(76, 88, 102);
    appearance.accent = palette::mint();
    plug_board_view_->set_appearance(appearance);
    plug_board_->set_icon_pixels(static_cast<unsigned>(FromDIP(32)));
}

void Frame::refresh_plug_board() {
    if (!plug_board_ || closing_)
        return;
    const auto& host = controller_->snapshot();
    const bool visible = IsShown() && !IsIconized() && notebook_->GetSelection() == PlugBoardPage;
    pb::EndpointInventory inventory;
    pb::Document inventory_key = pb::Document::array();
    for (const auto& known_device : host.endpoints) {
        const auto* device = &known_device;
        if (device->capture)
            continue;
        const std::string binding = plug_simulation_ ? device->id
                                    : device->inspection
                                        ? device->inspection->diagnostic_endpoint_id
                                        : std::string{};
        const bool available = device->availability == Availability::Available;
        const bool ambiguous = device->availability == Availability::Ambiguous;
        inventory.endpoints.push_back(
            {device->id, device->display_name, binding, available, ambiguous, true});
        inventory_key.push_back({device->id, device->display_name, binding, available, ambiguous});
    }
    const auto key = inventory_key.dump();
    if (key != plug_inventory_key_ || (visible && !plug_visible_)) {
        plug_inventory_key_ = key;
        inventory.revision = ++plug_inventory_revision_;
        if (plug_simulation_)
            inventory.default_windows_endpoints = {plug_simulated_default_, plug_simulated_default_,
                                                   plug_simulated_default_};
        plug_board_->set_inventory(std::move(inventory));
    }
    if (visible != plug_visible_) {
        plug_visible_ = visible;
        plug_board_view_->set_host_visible(visible);
    }
    pb::Document profile_key = pb::Document::array();
    std::vector<pb::ProfileDescriptor> profiles;
    for (const auto& profile : host.profiles) {
        profiles.push_back({profile.id, profile.display_name});
        profile_key.push_back({profile.id, profile.display_name});
    }
    if (profile_key.dump() != plug_profiles_key_) {
        plug_profiles_key_ = profile_key.dump();
        // Partial/rejected reloads are not deletion commands. Actual successful
        // GUI deletions call profile_deleted at their command boundary.
        plug_board_->set_profiles(std::move(profiles));
    }
    // A viewed profile, draft, pending switch or rejected admission cannot
    // activate an associated board. Successful re-Apply has a new live intent.
    if (host.armed && !host.routing_pending && !host.active_profile.empty() &&
        (host.state == State::Ready || host.state == State::Running ||
         host.state == State::Degraded) &&
        (host.active_profile != plug_active_profile_ ||
         host.routing_intent != plug_active_intent_)) {
        plug_active_profile_ = host.active_profile;
        plug_active_intent_ = host.routing_intent;
        plug_board_->profile_activated(host.active_profile, ++plug_activation_generation_);
    }
    const auto state = plug_board_->snapshot();
    if (!state || !state->library || (plug_seen_ && state->publication == plug_seen_->publication))
        return;
    if (tutorial_) {
        if (const auto* board = pb::find_board(*state->library, state->selected_board_id)) {
            if (plug_seen_ && state->last_command && state->last_command->ok &&
                (plug_seen_->library->revision != state->library->revision ||
                 plug_seen_->selected_board_id != state->selected_board_id))
                reflect_practice_playback(*plug_simulation_, *state, *board);
            TutorialPlugBoard practice;
            for (const auto& item : board->working.sockets)
                practice.sockets.push_back(
                    {item.id, item.display_name, item.endpoint.logical_device_id.value_or("")});
            practice.assignments = board->working.application_assignments;
            for (const auto& app : state->library->applications) {
                if (app.display_name == "Spooty")
                    practice.music_app = app.id;
                if (app.display_name == "Felony Jaywalking LXVII")
                    practice.game_app = app.id;
            }
            tutorial_->plug_board(practice);
            render_tutorial();
        }
    } else if (plug_seen_ && state->last_command && state->last_command->ok &&
               (!plug_seen_->last_command ||
                state->last_command->operation_id != plug_seen_->last_command->operation_id)) {
        if (const auto message = edit_message(*plug_seen_, *state))
            present(*message);
    }
    plug_seen_ = state;
}

wxWindow* Frame::plug_tutorial_target(const std::string& key) {
    if (!plug_board_view_)
        return nullptr;
    if (key == "plug_board_tab")
        return navigation_[PlugBoardPage];
    if (key == "plug_create") {
        plug_board_view_->set_drawer_open(true);
        return wxWindow::FindWindowByName("New socket", plug_board_view_);
    }
    if (key == "plug_columns")
        return wxWindow::FindWindowByName("Socket workspace", plug_board_view_);
    const auto layout = plug_board_view_->layout_diagnostics();
    const auto separator = key.find(':');
    if (separator == std::string::npos)
        return nullptr;
    const auto id = key.substr(separator + 1);
    if (!key.starts_with("plug_app:"))
        plug_board_view_->ensure_socket_visible(id);
    for (const auto& column : layout.sockets) {
        const bool app = key.starts_with("plug_app:");
        if (app ? std::find(column.rendered_application_ids.begin(),
                            column.rendered_application_ids.end(),
                            id) != column.rendered_application_ids.end()
                : column.id == id) {
            auto* control = wxWindow::FindWindowById(key.starts_with("plug_endpoint:")
                                                         ? column.endpoint_control_id
                                                         : column.applications_control_id,
                                                     plug_board_view_);
            if (key.starts_with("plug_rename:")) {
                return wxWindow::FindWindowByName(text("Socket heading " + id), plug_board_view_);
            }
            if (key.starts_with("plug_socket:"))
                return control ? control->GetParent() : nullptr;
            return control;
        }
    }
    return nullptr;
}
} // namespace audiohole::gui
