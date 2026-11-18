#pragma once

#include "view_state.hpp"

#include <atomic>
#include <thread>
#include <wx/taskbar.h>

namespace audiohole::plug_board::desktop {
struct BoardSmokeDriver;

// The standalone executable is a host shim. The reusable view owns no frame,
// scanner or tray. This frame supplies those application-lifetime services.
class BoardFrame final : public wxFrame {
  public:
    BoardFrame(ModuleOptions, bool hidden = false, ViewOptions = {});
    ~BoardFrame() override;
    WXLRESULT MSWWindowProc(WXUINT, WXWPARAM, WXLPARAM) override;
    void start_smoke(const std::filesystem::path&, std::function<void(int)>);

  private:
    friend struct BoardSmokeDriver;
    std::unique_ptr<Module> service;
    std::unique_ptr<wxTaskBarIcon> tray;
    std::shared_ptr<ViewMailbox> mailbox = std::make_shared<ViewMailbox>();
    std::uint64_t subscription = 0;
    std::shared_ptr<const ModuleSnapshot> snapshot;
    std::unique_ptr<BoardSmokeDriver> smoke;
    std::filesystem::path data_root, local_root;
    wxPanel* surface{};
    PlugBoardView* view{};
    Choice* appearance_choice{};
    wxStaticText* status{};
    wxString status_message;
    Button* refresh{};
    wxTimer completion_timer{this}, smoke_timer{this};
    std::vector<std::future<CommandResult>> repairs;
    std::shared_future<OperationResult> shutdown;
    Appearance colors;
    bool exiting = false;
    void build();
    void receive(wxThreadEvent&);
    void render();
    void set_status(const wxString&);
    void update_visibility();
    void apply_colors();
    void poll();
    void exit();
    void close(wxCloseEvent&);
};

// Developer-only injected simulation, exercised through current controls. It
// never constructs the Windows policy backend or writes the user's data root.
struct BoardSmokeDriver {
    ~BoardSmokeDriver();
    std::filesystem::path output;
    std::function<void(int)> finished;
    std::vector<std::string> assertions;
    std::string error;
    unsigned stage = 0;
    unsigned tick_count = 0;
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    std::string board_id, socket_id, application_id;
    wxSize original_size;
    wxPoint original_scroll;
    std::uint64_t socket_drag_revision = 0;
    std::size_t sockets_before_delete = 0;
    std::size_t menu_count = 0, name_count = 0, menu_count_before = 0, name_count_before = 0;
    bool last_socket_delete_enabled = true;
    unsigned style_index = 0;
    std::uint64_t style_policy_writes = 0;
    std::string opened_uri;
    std::optional<std::size_t> menu_selection;
    std::string next_name = "Smoke Board";
    // Only the copied screen points cross to the input producer. It owns a
    // bounded gesture and releases buttons before joining during teardown.
    std::jthread pointer_input;
    std::atomic<bool> pointer_finished{true};
    std::atomic<bool> pointer_failed{false};
    void* pointer_hook = nullptr;
    bool pointer_delivered = true; // GUI thread only, acknowledged by its local mouse hook.
    void pointer_gesture(wxPoint source, std::optional<wxPoint> destination, bool cancel = false);
    ViewOptions hooks();
    void verify_statuses(BoardFrame&);
    void tick(BoardFrame&);
    void finish(BoardFrame&);
};
} // namespace audiohole::plug_board::desktop
