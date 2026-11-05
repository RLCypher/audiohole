#include "gui/controls.h"
#include "gui/frame.h"
#include <wx/ffile.h>

namespace audiohole::gui {
void Frame::build_scan() {
    auto* page = new wxPanel(notebook_);
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* options = new wxBoxSizer(wxHORIZONTAL);
    scan_mode_ = choices(page, options, "Testing level", {"Basic / Easy", "Deep / Advanced"});
    scan_scope_ = choices(page, options, "Scope", {"Update (new devices)", "Full scan"});
    scan_mode_->SetToolTip(
        "Basic reads Windows-reported capabilities without experimental streams. Deep adds bounded "
        "stream tests with explicit consent. Preview simulates both.");
    scan_scope_->SetToolTip(
        "Update tests only newly discovered devices and refreshes known metadata. Full retests "
        "all. Per-device scans are available by right-clicking a device on Devices.");
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
    auto* attempts_column = new wxBoxSizer(wxVERTICAL);
    label(pane, attempts_column, "Attempts / device");
    deep_attempts_ = new wxSpinCtrl(pane, wxID_ANY, {}, wxDefaultPosition, wxSize(110, -1));
    deep_attempts_->SetRange(1, 64);
    deep_attempts_->SetValue(8);
    deep_attempts_->SetToolTip("Maximum tested candidate combinations per device, from 1 to 64.");
    attempts_column->Add(deep_attempts_, 0, wxALL, 3);
    budgets->Add(attempts_column);
    auto seconds = [&](const char* title, unsigned minimum, unsigned maximum, unsigned initial) {
        auto* column = new wxBoxSizer(wxVERTICAL);
        label(pane, column, title);
        auto* control = new SecondsField(pane, minimum, maximum, initial);
        control->SetName(text(title));
        control->editor()->SetName(text(title));
        column->Add(control, 0, wxALL | wxEXPAND, 3);
        budgets->Add(column, 1, wxEXPAND);
        return control;
    };
    deep_duration_ = seconds("Stream (s)", 250, 30000, 2000);
    deep_attempt_timeout_ = seconds("Attempt limit (s)", 1250, 120000, 10000);
    deep_device_timeout_ = seconds("Device limit (s)", 1000, 600000, 120000);
    deep_system_timeout_ = seconds("Scan limit (s)", 1000, 3600000, 600000);
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
    auto* filters = new wxBoxSizer(wxHORIZONTAL);
    auto* severity_column = new wxBoxSizer(wxVERTICAL);
    auto* search_column = new wxBoxSizer(wxVERTICAL);
    severity_ = choices(page, severity_column, "Diagnostic filter",
                        {"All events", "Errors only", "Routing state only"});
    log_search_ = field(page, search_column, "Search technical diagnostics");
    filters->Add(severity_column, 0, wxEXPAND);
    filters->Add(search_column, 1, wxEXPAND);
    root->Add(filters, 0, wxEXPAND);
    console_ = new wxTextCtrl(page, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    console_->SetMinSize(wxSize(0, 24));
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
    if (device_dirty_) {
        show_result({false, "Save device edits or reload/discard them before scanning."});
        return;
    }
    ScanOptions options;
    options.mode = utf8(deep_mode_->GetStringSelection());
    options.attempts = static_cast<unsigned>(deep_attempts_->GetValue());
    const auto duration = deep_duration_->milliseconds(),
               attempt = deep_attempt_timeout_->milliseconds(),
               device = deep_device_timeout_->milliseconds(),
               batch = deep_system_timeout_->milliseconds();
    if (deep && (!duration || !attempt || !device || !batch)) {
        show_result({false, "Enter seconds within each field's range, with up to three decimal "
                            "places (for example 2 or 2.125)."});
        return;
    }
    if (deep && *attempt < *duration + 1000) {
        show_result(
            {false,
             "The attempt limit must be at least 1 second longer than the stream duration."});
        return;
    }
    options.duration_ms = duration.value_or(2000);
    options.attempt_timeout_ms = attempt.value_or(10000);
    options.device_timeout_ms = device.value_or(120000);
    options.system_timeout_ms = batch.value_or(600000);
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
        if (wxMessageBox(
                text(
                    "Deep scan target: " + target + "\nMode: " + options.mode +
                    "\n\nFor best results, close applications that may have active audio streams: "
                    "browsers, "
                    "game launchers such as Steam and Epic, and chat apps such as Discord and "
                    "Telegram. "
                    "Close other nonessential applications; keep anything critical to your audio "
                    "workflow open. "
                    "Record that setup in the operating-context field.\n\n"
                    "Scan time varies with depth, number of devices, candidate attempts and "
                    "device/driver behavior. "
                    "Stream duration applies to each accepted test; setup and cleanup add time.\n\n"
                    "Testing may interrupt audio or produce sound. Lower listening volume and "
                    "prepare the device. "
                    "Short tests provide limited stream evidence. Ready to begin?"),
                "Ready to scan", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
            return;
        options.consent = true;
    }
    controller_->scan(true, deep, scope, id, options);
    refresh();
}
void Frame::update_console(const wxString& output) {
    const auto handle = static_cast<HWND>(console_->GetHandle());
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    const bool known = GetScrollInfo(handle, SB_VERT, &info) != FALSE;
    const bool follow = console_->IsEmpty() || !known ||
                        info.nMax <= static_cast<int>(info.nPage) ||
                        info.nPos >= std::max(0, info.nMax - static_cast<int>(info.nPage) + 1);
    const auto first_line = static_cast<long>(SendMessageW(handle, EM_GETFIRSTVISIBLELINE, 0, 0));
    const auto horizontal = ::GetScrollPos(handle, SB_HORZ);
    const auto first_position = console_->XYToPosition(0, first_line);
    const auto anchor =
        first_position >= 0
            ? console_->GetRange(first_position,
                                 std::min(first_position + 200, console_->GetLastPosition()))
            : wxString{};
    long selection_start = 0, selection_end = 0;
    console_->GetSelection(&selection_start, &selection_end);
    const auto old = console_->GetValue();
    console_->Freeze();
    if (output.StartsWith(old))
        console_->AppendText(output.Mid(old.length()));
    else
        console_->ChangeValue(output);
    const auto last = console_->GetLastPosition();
    console_->SetSelection(std::min(selection_start, last), std::min(selection_end, last));
    if (follow)
        console_->ShowPosition(last);
    else {
        long target_line = first_line;
        const auto match = anchor.empty() ? wxNOT_FOUND : output.Find(anchor);
        if (match != wxNOT_FOUND)
            // wx strings use LF; native edit positions include CRLF. Count lines
            // instead of passing a string offset to the Windows text control.
            target_line = static_cast<long>(output.Left(match).Freq('\n'));
        else
            target_line =
                0; // The bounded log evicted the old anchor; keep the oldest retained text visible.
        const auto current = static_cast<long>(SendMessageW(handle, EM_GETFIRSTVISIBLELINE, 0, 0));
        SendMessageW(handle, EM_LINESCROLL, horizontal - ::GetScrollPos(handle, SB_HORZ),
                     target_line - current);
    }
    console_->Thaw();
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
