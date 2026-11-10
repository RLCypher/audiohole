#include "gui/controls.h"
#include "gui/frame.h"
#include "model/configuration_bundle.h"
#include <wx/datetime.h>
#include <wx/ffile.h>

namespace audiohole::gui {
void Frame::bundle_dialog(bool profiles, bool importing) {
    if (dirty_ || device_dirty_) {
        show_result(
            {false, "Save or revert pending profile/device edits before transferring a bundle."});
        return;
    }
    const auto title =
        wxString(importing ? "Import " : "Export ") + (profiles ? "profiles" : "devices");
    wxFileDialog dialog(this, title, {},
                        wxString("AudioHole-") + (profiles ? "profiles-" : "devices-") +
                            wxDateTime::Now().Format("%Y%m%d-%H%M%S") + ".zip",
                        "AudioHole bundles (*.zip)|*.zip",
                        importing ? wxFD_OPEN | wxFD_FILE_MUST_EXIST : wxFD_SAVE);
    if (dialog.ShowModal() != wxID_OK)
        return;
    if (!importing && wxFileExists(dialog.GetPath())) {
        show_result({false, "That destination already exists. Choose a new filename to preserve "
                            "the existing bundle."});
        return;
    }
    ScannerActionRequest request;
    request.kind =
        importing
            ? (profiles ? ScannerActionKind::InspectProfiles : ScannerActionKind::InspectDevices)
            : (profiles ? ScannerActionKind::ExportProfiles : ScannerActionKind::ExportDevices);
    request.output = std::filesystem::path(dialog.GetPath().ToStdWstring());
    const auto result = controller_->scanner_action(request);
    if (!result.ok) {
        show_result(result);
        return;
    }
    bundle_generation_ = controller_->snapshot().scanner.generation;
    bundle_inspecting_ = importing;
    bundle_importing_ = false;
    // Freeze profile edits during a profile-file transaction/preview. Unfinished
    // drafts cannot race the worker and then be discarded by the reload.
    refresh();
}
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
    auto* deep = new wxCollapsiblePane(page, wxID_ANY, "Deep scan limits and test context");
    deep->SetName("Deep scan settings");
    auto* pane = deep->GetPane();
    auto* limits = new wxBoxSizer(wxVERTICAL);
    deep_mode_ = choices(pane, limits, "Operating mode", {"shared", "exclusive", "both"});
    deep_mode_->SetToolTip("Select shared, exclusive, or both for bounded Deep tests; exclusive "
                           "tests can conflict with other applications.");
    auto* budgets = new wxGridSizer(1, 5, FromDIP(8), FromDIP(8));
    auto* attempts_column = new wxBoxSizer(wxVERTICAL);
    auto title = [pane](wxSizer* column, const wxString& caption) {
        auto* text = new wxStaticText(pane, wxID_ANY, caption, wxDefaultPosition, wxDefaultSize,
                                      wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_END);
        text->SetMinSize(wxSize(0, text->GetCharHeight() * 2));
        column->Add(text, 0, wxEXPAND | wxALL, 3);
    };
    title(attempts_column, "Configurations / device");
    deep_attempts_ = new wxSpinCtrl(pane, wxID_ANY, {}, wxDefaultPosition, wxSize(110, -1));
    deep_attempts_->SetRange(1, 64);
    deep_attempts_->SetName("Configurations per device");
    deep_attempts_->SetValue(8);
    deep_attempts_->SetToolTip(
        "How many distinct format/mode combinations to test on each device. "
        "More configurations broaden coverage and increase scan time; these are not retries.");
    attempts_column->Add(deep_attempts_, 0, wxALL | wxEXPAND, 3);
    budgets->Add(attempts_column, 1, wxEXPAND);
    auto seconds = [&](const char* caption, unsigned minimum, unsigned maximum, unsigned initial) {
        auto* column = new wxBoxSizer(wxVERTICAL);
        title(column, text(caption));
        auto* control = new SecondsField(pane, minimum, maximum, initial);
        control->SetName(text(caption));
        control->editor()->SetName(text(caption));
        column->Add(control, 0, wxALL | wxEXPAND, 3);
        budgets->Add(column, 1, wxEXPAND);
        return control;
    };
    deep_duration_ = seconds("Stream (sec.)", 250, 30000, 2000);
    deep_attempt_timeout_ = seconds("Attempt limit (sec.)", 1250, 120000, 10000);
    deep_device_timeout_ = seconds("Device limit (sec.)", 1000, 600000, 120000);
    deep_system_timeout_ = seconds("Scan limit (sec.)\n0 = unlimited", 0, 3600000, 600000);
    const std::array<std::pair<SecondsField*, const char*>, 4> descriptions{
        {{deep_duration_, "How long each accepted configuration processes audio buffers. Longer "
                          "tests observe more activity but do not prove long-term stability."},
         {deep_attempt_timeout_,
          "Maximum worker time for one configuration, including stream setup, processing and "
          "cleanup. Reporting/checkpoint time is separate."},
         {deep_device_timeout_, "Total Deep test budget for one device. When used up, remaining "
                                "configurations are left untested and scanning moves on safely."},
         {deep_system_timeout_,
          "Overall Deep scan budget across all selected devices. Zero disables this deadline; "
          "individual attempt/device limits and Cancel remain active."}}};
    for (const auto& [control, description] : descriptions) {
        control->SetToolTip(description);
        for (auto* child : control->GetChildren())
            child->SetToolTip(description);
    }
    const int control_height = deep_attempts_->GetBestSize().y;
    for (auto* control :
         {deep_duration_, deep_attempt_timeout_, deep_device_timeout_, deep_system_timeout_})
        control->SetMinSize(wxSize(FromDIP(100), control_height));
    limits->Add(budgets, 0, wxEXPAND | wxALL, FromDIP(3));
    scan_context_ = field(pane, limits,
                          "Optional operating context (other apps, device setup, observed issue)");
    scan_context_->SetMaxLength(8192);
    scan_context_->SetToolTip("Stored with this scan request for later analysis; maximum 8192 "
                              "UTF-8 bytes. Review before sharing a bundle.");
    pane->SetSizer(limits);
    root->Add(deep, 0, wxEXPAND | wxALL, 3);
    deep->Show(controller_->snapshot().operating_mode == OperatingMode::Test);
    deep->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED, [page](wxCollapsiblePaneEvent&) { page->Layout(); });
    scan_progress_ =
        new wxGauge(page, wxID_ANY, 100, wxDefaultPosition, wxDefaultSize, wxGA_SMOOTH);
    root->Add(scan_progress_, 0, wxEXPAND | wxALL, 4);
    scan_output_split_ = new wxSplitterWindow(
        page, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(400, 240)), wxSP_LIVE_UPDATE | wxSP_3D);
    scan_output_split_->SetName("Resize scan status height");
    scan_output_split_->SetToolTip(
        "Drag the divider below the status to resize it. Long status text can also scroll.");
    scan_output_split_->SetSashGravity(0);
    scan_output_split_->SetMinimumPaneSize(FromDIP(48));
    scan_output_split_->SetMinSize(FromDIP(wxSize(0, 120)));
    auto* status_panel = new wxPanel(scan_output_split_);
    auto* status_layout = new wxBoxSizer(wxVERTICAL);
    auto* log_panel = new wxPanel(scan_output_split_);
    auto* log_layout = new wxBoxSizer(wxVERTICAL);
    scan_state_ = new wxTextCtrl(status_panel, wxID_ANY, "No scan requested.", wxDefaultPosition,
                                 wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    scan_state_->SetName("Current scan status");
    const int status_height = scan_state_->GetCharHeight() * 3 + FromDIP(8);
    scan_state_->SetMinSize(wxSize(0, scan_state_->GetCharHeight() + FromDIP(8)));
    status_layout->Add(scan_state_, 1, wxEXPAND | wxALL, FromDIP(4));
    status_panel->SetSizer(status_layout);
    auto* filters = new wxBoxSizer(wxHORIZONTAL);
    auto* severity_column = new wxBoxSizer(wxVERTICAL);
    auto* search_column = new wxBoxSizer(wxVERTICAL);
    severity_ = choices(log_panel, severity_column, "Diagnostic filter",
                        {"All events", "Errors only", "Routing state only"});
    log_search_ = field(log_panel, search_column, "Search technical diagnostics");
    filters->Add(severity_column, 0, wxEXPAND);
    filters->Add(search_column, 1, wxEXPAND);
    log_layout->Add(filters, 0, wxEXPAND);
    console_ = new wxTextCtrl(log_panel, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    console_->SetMinSize(wxSize(0, 24));
    log_layout->Add(console_, 1, wxEXPAND | wxALL, FromDIP(4));
    log_panel->SetSizer(log_layout);
    scan_output_split_->SplitHorizontally(status_panel, log_panel, status_height + FromDIP(8));
    scan_output_split_->Bind(wxEVT_SPLITTER_DOUBLECLICKED,
                             [](wxSplitterEvent& event) { event.Veto(); });
    scan_output_split_->Bind(
        wxEVT_SPLITTER_SASH_POS_CHANGING, [this, log_layout](wxSplitterEvent& event) {
            // The status can be compact, but dragging must leave the log's filter
            // controls and at least its minimum text area available.
            const int maximum = scan_output_split_->GetClientSize().y -
                                scan_output_split_->GetSashSize() - log_layout->GetMinSize().y;
            if (maximum >= scan_output_split_->GetMinimumPaneSize())
                event.SetSashPosition(std::clamp(
                    event.GetSashPosition(), scan_output_split_->GetMinimumPaneSize(), maximum));
        });
    root->Add(scan_output_split_, 1, wxEXPAND | wxALL, FromDIP(4));
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
        if (themed_message_box(
                "The bundle includes device identifiers, local bindings, notes and test "
                "context. Review it before sharing.",
                "Analysis bundle", wxOK | wxCANCEL | wxICON_INFORMATION, this) != wxOK)
            return;
        const auto filename =
            wxS("AudioHole-scan-") + wxDateTime::Now().Format("%Y%m%d-%H%M%S") + wxS(".zip");
        wxFileDialog dialog(this, "Export last scan session", {}, filename,
                            "ZIP archives (*.zip)|*.zip", wxFD_SAVE);
        theme_dialog(&dialog);
        if (dialog.ShowModal() != wxID_OK)
            return;
        while (wxFileExists(dialog.GetPath())) {
            themed_message_box(
                "That ZIP already exists. Choose a different name to preserve the existing bundle.",
                "Export scan bundle", wxOK | wxICON_INFORMATION, this);
            theme_dialog(&dialog);
            if (dialog.ShowModal() != wxID_OK)
                return;
        }
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
    unsigned long attempt_count = 0;
    if (deep && (!deep_attempts_->GetTextValue().ToULong(&attempt_count) || attempt_count < 1 ||
                 attempt_count > 64)) {
        show_result({false, "Enter a whole configuration count from 1 to 64 per device."});
        return;
    }
    options.attempts = deep ? static_cast<unsigned>(attempt_count) : 8;
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
    if (deep && *batch != 0 && *batch < 1000) {
        show_result(
            {false, "Use 0 for unlimited total scan time, or a scan limit of at least 1 second."});
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
        if (themed_message_box(
                text(
                    "Power on all devices you want to use, including headsets, monitors, mixers "
                    "and VR devices. Ensure they are operational.\n\nDeep scan target: " +
                    target + "\nMode: " + options.mode + "\nOverall scan limit: " +
                    (options.system_timeout_ms == 0
                         ? std::string("Unlimited (attempt/device limits remain active)")
                         : std::to_string(options.system_timeout_ms / 1000) + " seconds") +
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
