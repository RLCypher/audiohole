#include "gui/controls.h"
#include "gui/frame.h"
#include "gui/welcome.h"
#include "model/configuration_controller.h"
#include "model/first_run.h"
#include <cstdio>
#include <cstdlib>
#include <shobjidl.h>
#include <string_view>
#include <wx/stdpaths.h>

namespace {
// Internal restart handshake: the child waits for the old process to terminate
// BEFORE acquiring the single-instance guard or loading any user data.
bool wait_for_restart_parent(DWORD parent) {
    if (!parent || parent == GetCurrentProcessId())
        return false;
    const auto process =
        OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parent);
    if (!process)
        return GetLastError() == ERROR_INVALID_PARAMETER; // Already exited.
    std::wstring image(32768, L'\0');
    DWORD length = static_cast<DWORD>(image.size());
    bool same_image = false;
    if (QueryFullProcessImageNameW(process, 0, image.data(), &length)) {
        image.resize(length);
        std::error_code error;
        same_image =
            std::filesystem::equivalent(
                std::filesystem::path(image),
                std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring()),
                error) &&
            !error;
    } else
        same_image = WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
    const bool exited = same_image && WaitForSingleObject(process, 15000) == WAIT_OBJECT_0;
    CloseHandle(process);
    return exited;
}
std::wstring quote_argument(const std::wstring& value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const auto character : value) {
        if (character == L'\\') {
            ++slashes;
            continue;
        }
        result.append(character == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        result += character;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    result += L'\"';
    return result;
}
bool launch_restart(bool smoke, const wxString& screenshot_dir) {
    const auto executable = wxStandardPaths::Get().GetExecutablePath().ToStdWstring();
    auto command =
        quote_argument(executable) + L" --restart-after " + std::to_wstring(GetCurrentProcessId());
    if (smoke)
        command += L" --smoke-test";
    if (!screenshot_dir.empty())
        command += L" --screenshot-dir " + quote_argument(screenshot_dir.ToStdWstring());
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        nullptr, &startup, &process))
        return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}
} // namespace

namespace audiohole::gui {
// Owns process-lifetime COM and the single-instance guard. The STA matches native
// GUI/OLE needs; future WASAPI workers initialize their own apartments.
class App final : public wxApp {
  public:
    bool OnInit() override {
        SetAppName("AudioHole");
        bool smoke = false, test_mode_smoke = false;
        wxString screenshot_dir;
        DWORD restart_parent = 0;
        std::optional<ShellAction> shell_action;
        for (int i = 1; i < argc; ++i) {
            if (wxString(argv[i]) == "--smoke-test")
                smoke = true;
            else if (wxString(argv[i]) == "--test-mode-smoke")
                smoke = test_mode_smoke = true;
            else if (wxString(argv[i]) == "--screenshot-dir" && i + 1 < argc)
                screenshot_dir = argv[++i];
            else if (wxString(argv[i]) == "--taskbar-action" && i + 1 < argc && !shell_action) {
                shell_action = parse_shell_action(wxString(argv[++i]).ToStdWstring());
                if (!shell_action)
                    return false;
            } else if (wxString(argv[i]) == "--restart-after" && i + 1 < argc) {
                unsigned long value = 0;
                if (!wxString(argv[++i]).ToULong(&value) || value == 0)
                    return false;
                restart_parent = static_cast<DWORD>(value);
            } else
                return false;
        }
        if (shell_action) {
            if (smoke || restart_parent)
                return false;
            if (const auto existing = find_application_window()) {
                (void)forward_shell_action(existing, *shell_action);
                return false;
            }
            // Persisted shell tasks do not authorize a fresh routing session.
            // Only Open / Controls may launch the ordinary startup workflow.
            if (*shell_action != ShellAction::Open && *shell_action != ShellAction::Controls)
                return false;
        }
        if (restart_parent && !wait_for_restart_parent(restart_parent)) {
            themed_message_box(
                "The previous AudioHole process did not shut down cleanly. No second "
                "session was started.",
                "AudioHole restart", wxOK | wxICON_ERROR);
            return false;
        }
        smoke_ = smoke;
        screenshot_dir_ = screenshot_dir;
        if (smoke) {
            // Unattended CI must fail visibly instead of hanging on a debug assert
            // or error dialog. Interactive users retain ordinary wx diagnostics.
            wxLog::SetActiveTarget(new wxLogStderr());
            wxSetAssertHandler([](const wxString& file, int line, const wxString& function,
                                  const wxString& condition, const wxString& message) {
                std::fprintf(stderr, "GUI assertion: %s:%d %s: %s (%s)\n", file.utf8_str().data(),
                             line, function.utf8_str().data(), condition.utf8_str().data(),
                             message.utf8_str().data());
                std::fflush(stderr);
                std::exit(3); // A disposable smoke process, never a normal user session.
            });
        }
        apartment_ = std::make_unique<audiohole::windows::ComApartment>(
            audiohole::windows::ApartmentModel::single_threaded);
        if (!apartment_->initialized()) {
            themed_message_box("COM initialization failed. AudioHole cannot start safely.",
                               "AudioHole", wxOK | wxICON_ERROR);
            return false;
        }
        if (!smoke) {
            (void)SetCurrentProcessExplicitAppUserModelID(shell_app_id);
            instance_ = std::make_unique<wxSingleInstanceChecker>("AudioHole-simulated-GUI-" +
                                                                  wxGetUserId());
            if (instance_->IsAnotherRunning()) {
                HWND existing = nullptr;
                // Build identifiers change the title; identify our window by its
                // stable property instead. Legacy titles support older test builds.
                EnumWindows(
                    [](HWND window, LPARAM context) -> BOOL {
                        wchar_t title[256]{};
                        GetWindowTextW(window, title, 256);
                        const std::wstring_view name(title);
                        const bool legacy = name.starts_with(L"AudioHole — Simulated routing");
                        if (!GetPropW(window, L"AudioHole.GuiWindow") && !legacy)
                            return TRUE;
                        *reinterpret_cast<HWND*>(context) = window;
                        return FALSE;
                    },
                    reinterpret_cast<LPARAM>(&existing));

                if (existing) {
                    ShowWindow(existing, SW_SHOW);
                    ShowWindow(existing, SW_RESTORE);
                    SetForegroundWindow(existing);
                }
                return false;
            }
        }
        auto root = smoke ? std::filesystem::path{}
                          : std::filesystem::path(
                                wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring()) /
                                "PreviewBuild";
        if (test_mode_smoke) {
            smoke_root_ = std::filesystem::temp_directory_path() /
                          ("AudioHole-mode-smoke-" + std::to_string(GetCurrentProcessId()) + "-" +
                           std::to_string(GetTickCount64()));
            if (!std::filesystem::create_directory(smoke_root_))
                return false;
            root = smoke_root_;
        }
        preferences_ = std::make_shared<BuildPreferences>(root, smoke);
        const auto preferences_result = preferences_->load();
        if (!preferences_result.ok && !smoke)
            themed_message_box(text(preferences_result.detail), "AudioHole preferences",
                               wxOK | wxICON_WARNING);
        bool preview = !test_mode_smoke && (smoke || preferences_->values().preview_mode);
        bool first_time = false;
        if (!smoke) {
            const auto checked = first_run_needed(first_run_marker_path(), first_time);
            if (!checked.ok)
                themed_message_box(text(checked.detail), "AudioHole welcome",
                                   wxOK | wxICON_WARNING);
        }
        if (!smoke && (first_time || (preferences_->values().show_preview_dialog &&
                                      !preferences_->values().start_minimized))) {
            WelcomeDialog dialog(first_time || preview);
            const auto choice = dialog.ShowModal();
            if (choice == wxID_CANCEL)
                return false;
            preview = choice == wxID_YES;
            auto next = preferences_->values();
            next.preview_mode = preview;
            const auto saved = preferences_->update(next);
            if (!saved.ok)
                themed_message_box(text(saved.detail), "AudioHole preferences",
                                   wxOK | wxICON_WARNING);
        }
        if (!smoke) {
            const auto marked = mark_first_run(first_run_marker_path());
            if (!marked.ok)
                themed_message_box(text(marked.detail), "AudioHole welcome", wxOK | wxICON_WARNING);
        }
        std::unique_ptr<Controller> controller;
        if (preview)
            controller = std::make_unique<SimulatedController>();
        else {
            ScannerServiceOptions scanner;
            scanner.build_id = AUDIOHOLE_BUILD_COMMIT;
            scanner.portable_root = preferences_->data_root(OperatingMode::Test);
            scanner.local_root = scanner.portable_root.parent_path() / "test-local";
            scanner.probe_executable =
                std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring())
                    .parent_path() /
                "audiohole-probe.exe";
            controller = std::make_unique<ConfigurationController>(
                std::make_unique<ProfileDirectoryPersistence>(preferences_->test_configuration()),
                std::make_unique<ScannerService>(std::move(scanner)));
        }
        auto* frame = new Frame(std::move(controller), smoke, preferences_, [this] {
            restart_requested_ = true;
            static_cast<Frame*>(GetTopWindow())->exit();
        });
        SetTopWindow(frame);
        frame->Show();
        if (!smoke && preferences_->values().start_minimized)
            frame->Iconize(true);
        if (shell_action == ShellAction::Controls)
            frame->CallAfter([frame] { frame->perform_shell_action(ShellAction::Controls); });
        if (smoke)
            CallAfter([this, frame, screenshot_dir] {
                result_ = frame->smoke(screenshot_dir) ? 0 : 1;
                frame->exit();
            });
        return true;
    }
    int OnExit() override {
        instance_.reset();
        apartment_.reset();
        if (!smoke_root_.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(smoke_root_, ignored);
        }
        return result_;
    }
    int OnRun() override {
        wxApp::OnRun();
        if (restart_requested_) {
            // All frames/timers have closed. Release the guard and COM before
            // launching; the child still waits for this process to exit fully.
            instance_.reset();
            apartment_.reset();
            if (!launch_restart(smoke_, screenshot_dir_)) {
                themed_message_box(
                    "AudioHole closed safely, but could not relaunch. Start it normally; "
                    "saved files are preserved.",
                    "AudioHole restart", wxOK | wxICON_ERROR);
                return 1;
            }
        }
        return result_;
    }

  private:
    std::unique_ptr<audiohole::windows::ComApartment> apartment_;
    std::unique_ptr<wxSingleInstanceChecker> instance_;
    int result_ = 0;
    bool restart_requested_ = false, smoke_ = false;
    wxString screenshot_dir_;
    std::filesystem::path smoke_root_;
    std::shared_ptr<BuildPreferences> preferences_;
};

} // namespace audiohole::gui
wxIMPLEMENT_APP(audiohole::gui::App);
