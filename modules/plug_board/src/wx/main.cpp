#include "audiohole/plug_board/simulation.hpp"
#include "audiohole/plug_board/windows.hpp"
#include "board_frame.hpp"
#include "dialogs.hpp"

#include <wx/app.h>
#include <wx/image.h>
#include <wx/stdpaths.h>

namespace audiohole::plug_board::desktop {

// The default Windows entry point opens the real window with no arguments.
// Developer smoke mode substitutes an explicit sample-data executor; the normal
// application always owns the native backend through its module service.
class LauncherApp final : public wxApp {
  public:
    bool OnInit() override {
        SetAppName("AudioHolePlugBoard");
        SetVendorName("AudioHole");
        wxStandardPaths::Get().UseAppInfo(wxStandardPaths::AppInfo_VendorName |
                                          wxStandardPaths::AppInfo_AppName);
        wxInitAllImageHandlers();
        std::filesystem::path smoke_output;
        std::filesystem::path data_root;
        bool board_smoke = false, demo = false, hidden = false;
        for (int index = 1; index < argc; ++index) {
            const wxString argument(argv[index]);
            if (argument == "--board-smoke") {
                smoke_ = true;
                board_smoke = true;
                demo = true;
            } else if (argument == "--demo") {
                demo = true;
            } else if (argument == "--hidden") {
                hidden = true;

            } else if (argument == "--data-root" && index + 1 < argc) {
                data_root = wxString(argv[++index]).ToStdWstring();
            } else if (argument == "--smoke-output" && index + 1 < argc) {
                smoke_output = wxString(argv[++index]).ToStdWstring();
            } else {
                show_message(nullptr,
                             "Open AudioHole-PlugBoard.exe without command-line arguments.",
                             "AudioHole Plug Board", wxOK | wxICON_INFORMATION,
                             appearance_for("Follow System"));
                return false;
            }
        }
        if (smoke_ && smoke_output.empty()) {
            return false;
        }
        if (!smoke_ && !smoke_output.empty()) {
            return false;
        }
        {
            ModuleOptions options;
            const auto base =
                std::filesystem::path(wxStandardPaths::Get().GetUserDataDir().ToStdWstring());
            options.data_root = board_smoke ? smoke_output / "isolated-data" / new_id("run")
                                : data_root.empty()
                                    ? base / (demo ? "plug-board-demo" : "plug-board")
                                    : data_root;
            options.local_root = options.data_root / "local";
            options.standalone_inventory = true;
            if (demo) {
                const auto simulation = make_demo_simulation();
                options.backend = [simulation] { return make_simulated_backend(simulation); };
                options.profiles = {{"demo-profile", "Example Profile"},
                                    {"demo-secondary", "Second Example"}};
            } else
                options.backend = [] { return make_windows_backend(true); };
            auto* frame = new BoardFrame(std::move(options), hidden && !board_smoke);
            SetTopWindow(frame);
            if (board_smoke)
                frame->start_smoke(smoke_output, [this](int result) { smoke_result_ = result; });
        }
        return true;
    }
    int OnRun() override {
        const int result = wxApp::OnRun();
        return smoke_ ? smoke_result_ : result;
    }

  private:
    bool smoke_ = false;
    int smoke_result_ = 1;
};

} // namespace audiohole::plug_board::desktop

wxIMPLEMENT_APP(audiohole::plug_board::desktop::LauncherApp);
