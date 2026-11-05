#include "gui/controls.h"
#include "gui/frame.h"
#include <wx/ffile.h>

namespace audiohole::gui {
void Frame::build_scan() {
    auto* page = new wxPanel(notebook_);
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* options = new wxBoxSizer(wxHORIZONTAL);
    scan_mode_ = choices(page, options, "Testing level", {"Basic / Easy", "Deep / Advanced"});
    scan_scope_ =
        choices(page, options, "Scope", {"Update (new devices)", "Full scan", "Selected device"});
    scan_mode_->SetToolTip(
        "Basic reads Windows-reported capabilities without experimental streams. Deep adds bounded "
        "stream tests with explicit consent. Preview simulates both.");
    scan_scope_->SetToolTip(
        "Update tests only newly discovered devices and refreshes known metadata. Full retests "
        "all. Selected targets the exact saved logical device from Devices.");
    scan_mode_->Bind(wxEVT_CHOICE,
                     [this](wxCommandEvent&) { present(presentation::Message::ScanLevelChanged); });
    scan_scope_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        present(presentation::Message::ScanScopeChanged);
    });
    root->Add(options, 0, wxEXPAND);
    auto* deep = new wxCollapsiblePane(page, wxID_ANY, "Deep scan limits and test context");
    auto* pane = deep->GetPane();
    auto* limits = new wxBoxSizer(wxVERTICAL);
    deep_mode_ = choices(pane, limits, "Operating mode", {"shared", "exclusive", "both"});
    deep_mode_->SetToolTip("Select shared, exclusive, or both for bounded Deep tests; exclusive "
                           "tests can conflict with other applications.");
    auto* budgets = new wxBoxSizer(wxHORIZONTAL);
    auto spin = [&](const char* title, int minimum, int maximum, int initial) {
        auto* column = new wxBoxSizer(wxVERTICAL);
        label(pane, column, title);
        auto* control = new wxSpinCtrl(pane, wxID_ANY, {}, wxDefaultPosition, wxSize(110, -1));
        control->SetRange(minimum, maximum);
        control->SetValue(initial);
        control->SetName(text(title));
        control->SetToolTip(text(
            std::string(title) + ": " + std::to_string(minimum) + "–" + std::to_string(maximum) +
            ". Attempt timeout must exceed stream duration by at least 1000 ms."));
        column->Add(control, 0, wxALL, 3);
        budgets->Add(column);
        return control;
    };
    deep_attempts_ = spin("Attempts / device", 1, 64, 8);
    deep_duration_ = spin("Stream ms", 250, 30000, 2000);
    deep_attempt_timeout_ = spin("Attempt ms", 1250, 120000, 10000);
    deep_device_timeout_ = spin("Device ms", 1000, 600000, 120000);
    deep_system_timeout_ = spin("Batch ms", 1000, 3600000, 600000);
    limits->Add(budgets);
    scan_context_ = field(pane, limits,
                          "Optional operating context (other apps, device setup, observed issue)");
    scan_context_->SetMaxLength(8192);
    scan_context_->SetToolTip("Stored with this scan request for later analysis; maximum 8192 "
                              "UTF-8 bytes. Review before sharing a bundle.");
    pane->SetSizer(limits);
    root->Add(deep, 0, wxEXPAND | wxALL, 3);
    deep->Show(controller_->snapshot().operating_mode == OperatingMode::Test);
    deep->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED, [page](wxCollapsiblePaneEvent&) { page->Layout(); });
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    scan_start_ = button(page, row, "Start scan", [this] {
        request_scan(scan_mode_->GetSelection() == 1,
                     static_cast<ScanScope>(scan_scope_->GetSelection()), device_id_);
    });
    scan_cancel_ = button(page, row, "Cancel scan", [this] {
        controller_->scan(false);
        refresh();
    });
    root->Add(row);
    scan_progress_ = new wxGauge(page, wxID_ANY, 100);
    root->Add(scan_progress_, 0, wxEXPAND | wxALL, 4);
    scan_state_ = label(page, root, "No scan requested.");
    severity_ = choices(page, root, "Diagnostic filter",
                        {"All events", "Errors only", "Routing state only"});
    log_search_ = field(page, root, "Search technical diagnostics");
    console_ = new wxTextCtrl(page, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    root->Add(console_, 1, wxEXPAND | wxALL, 4);
    auto* export_row = new wxBoxSizer(wxHORIZONTAL);
    button(page, export_row, "Copy visible diagnostics", [this] {
        if (!wxTheClipboard->Open()) {
            show_result(
                {false,
                 "Could not open the clipboard. Another application may be using it. Try again."});
            return;
        }
        // wxClipboard owns the supplied data object. Close on either SetData
        // outcome; never announce success when the clipboard rejects the data.
        const bool copied = wxTheClipboard->SetData(new wxTextDataObject(
            (controller_->snapshot().operating_mode == OperatingMode::Preview
                 ? wxS("AudioHole Preview Build — simulated data; no hardware measurements.\n\n")
                 : wxS("AudioHole Test Mode — real scanner evidence; no routing/playback "
                       "backend.\n\n")) +
            console_->GetValue()));
        wxTheClipboard->Close();
        if (copied)
            present(presentation::Message::DiagnosticsCopied);
        else
            show_result({false, "Could not copy diagnostics to the clipboard. Try again."});
    });
    button(page, export_row, "Export visible diagnostics…", [this] { export_log(); });
    button(page, export_row, "Clear diagnostics log", [this] {
        controller_->clear_diagnostics();
        avatar_diagnostics_.clear();
        refresh();
        present(presentation::Message::DiagnosticsCleared);
    });
    auto* bundle = button(page, export_row, "Export scan bundle…", [this] {
        if (wxMessageBox("The bundle includes device identifiers, local bindings, notes and test "
                         "context. Review it before sharing.",
                         "Analysis bundle", wxOK | wxCANCEL | wxICON_INFORMATION, this) != wxOK)
            return;
        wxFileDialog dialog(this, "Export last scan session", {}, "AudioHole-scan.zip",
                            "ZIP archives (*.zip)|*.zip", wxFD_SAVE);
        if (dialog.ShowModal() != wxID_OK)
            return;
        ScannerActionRequest request;
        request.kind = ScannerActionKind::Export;
        request.output = std::filesystem::path(dialog.GetPath().ToStdWstring());
        scanner_action(request);
    });
    bundle->SetToolTip("Export the last scan's request, progress, results and device records to a "
                       "new archive. Existing files are never overwritten.");
    scanner_actions_.push_back(bundle);
    root->Add(export_row);
    page->SetSizer(root);
    notebook_->AddPage(page, "Scanning / Diagnostics");
}
// Every entry point shares the same exact target and consent path. Widgets issue
// typed requests; only the background scanner opens Windows devices or files.
void Frame::request_scan(bool deep, ScanScope scope, const std::string& id) {
    ScanOptions options;
    options.mode = utf8(deep_mode_->GetStringSelection());
    options.attempts = static_cast<unsigned>(deep_attempts_->GetValue());
    options.duration_ms = static_cast<unsigned>(deep_duration_->GetValue());
    options.attempt_timeout_ms = static_cast<unsigned>(deep_attempt_timeout_->GetValue());
    options.device_timeout_ms = static_cast<unsigned>(deep_device_timeout_->GetValue());
    options.system_timeout_ms = static_cast<unsigned>(deep_system_timeout_->GetValue());
    options.operating_context = utf8(scan_context_->GetValue());
    const auto* selected = endpoint(controller_->snapshot(), id);
    if (scope == ScanScope::Selected && !selected) {
        show_result({false, "Select a saved device on Devices first."});
        return;
    }
    if (deep) {
        const auto target = scope == ScanScope::Selected ? selected->display_name + " [" + id + "]"
                            : scope == ScanScope::Full
                                ? std::string("all eligible devices")
                                : std::string("newly discovered devices only");
        if (wxMessageBox(text("Deep scan target: " + target + "\nMode: " + options.mode +
                              "\n\nTesting may interrupt audio or produce sound. Lower listening "
                              "volume and prepare the device. "
                              "Short stream evidence does not establish sustained reliability or "
                              "recommended settings. Continue?"),
                         "Deep scan consent", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                         this) != wxYES)
            return;
        options.consent = true;
    }
    controller_->scan(true, deep, scope, id, options);
    refresh();
}
void Frame::export_log() {
    wxFileDialog dialog(this, "Export technical diagnostics", {}, "AudioHole-diagnostics.txt",
                        "Text files (*.txt)|*.txt", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return;
    // On-screen wording previews production; copied/exported evidence must retain
    // simulator provenance after it leaves the context of the startup notice.
    wxFFile output(dialog.GetPath(), "wb");
    if (!output.IsOpened() ||
        !output.Write(
            (controller_->snapshot().operating_mode == OperatingMode::Preview
                 ? wxS("AudioHole Preview Build — simulated data; no hardware measurements.\n\n")
                 : wxS("AudioHole Test Mode — real scanner evidence; no routing/playback "
                       "backend.\n\n")) +
                console_->GetValue(),
            wxConvUTF8) ||
        !output.Close())
        show_result({false, "Could not write diagnostics to the selected file. Check permissions "
                            "and available space."});
    else
        present(presentation::Message::DiagnosticsExported);
}

} // namespace audiohole::gui
