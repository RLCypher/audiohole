#include "gui/frame.h"
#include <cstdio>
#include <cstdlib>

namespace audiohole::gui {
// Owns process-lifetime COM and the single-instance guard. The STA matches native
// GUI/OLE needs; future WASAPI workers initialize their own apartments.
class App final : public wxApp {
  public:
    bool OnInit() override {
        bool smoke = false;
        wxString screenshot_dir;
        for (int i = 1; i < argc; ++i) {
            if (wxString(argv[i]) == "--smoke-test")
                smoke = true;
            else if (wxString(argv[i]) == "--screenshot-dir" && i + 1 < argc)
                screenshot_dir = argv[++i];
            else
                return false;
        }
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
            wxMessageBox("COM initialization failed. AudioHole cannot start safely.", "AudioHole",
                         wxOK | wxICON_ERROR);
            return false;
        }
        if (!smoke) {
            instance_ = std::make_unique<wxSingleInstanceChecker>("AudioHole-simulated-GUI-" +
                                                                  wxGetUserId());
            if (instance_->IsAnotherRunning()) {
                if (HWND window = FindWindowW(nullptr, L"AudioHole — Simulated routing")) {
                    ShowWindow(window, SW_SHOW);
                    ShowWindow(window, SW_RESTORE);
                    SetForegroundWindow(window);
                }
                return false;
            }
        }
        auto* frame = new Frame(std::make_unique<audiohole::SimulatedController>(), smoke);
        SetTopWindow(frame);
        frame->Show();
        if (!smoke)
            CallAfter([frame] { frame->show_introduction(); });
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
        return result_;
    }
    int OnRun() override {
        wxApp::OnRun();
        return result_;
    }

  private:
    std::unique_ptr<audiohole::windows::ComApartment> apartment_;
    std::unique_ptr<wxSingleInstanceChecker> instance_;
    int result_ = 0;
};

} // namespace audiohole::gui
wxIMPLEMENT_APP(audiohole::gui::App);
