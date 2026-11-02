#include "gui/controls.h"
#include "gui/frame.h"
#include <wx/ffile.h>

namespace audiohole::gui {
void Frame::build_scan() {
    auto* page = new wxPanel(notebook_);
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* options = new wxBoxSizer(wxHORIZONTAL);
    scan_mode_ = choices(page, options, "Testing level", {"Easy", "Advanced"});
    scan_scope_ =
        choices(page, options, "Scope", {"Update saved devices", "Full scan", "Selected device"});
    scan_mode_->SetToolTip(
        "Easy selects the nonintrusive scan scenario. Advanced requests explicit consent for "
        "potentially disruptive testing. Both levels currently simulate progress; neither opens "
        "hardware or verifies device capabilities.");
    scan_scope_->SetToolTip(
        "Scope chooses the target: saved records, a full discovery scenario, or the device "
        "selected on Devices. Level chooses testing intensity instead. This preview records the "
        "chosen scope and simulates progress; it does not enumerate hardware.");
    scan_mode_->Bind(wxEVT_CHOICE,
                     [this](wxCommandEvent&) { present(presentation::Message::ScanLevelChanged); });
    scan_scope_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        present(presentation::Message::ScanScopeChanged);
    });
    root->Add(options, 0, wxEXPAND);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    scan_start_ = button(page, row, "Start scan", [this] {
        if (scan_mode_->GetSelection() == 1 &&
            wxMessageBox("Advanced testing can interrupt devices or produce sound. Continue?",
                         "Advanced scan consent", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                         this) != wxYES)
            return;
        controller_->scan(true, scan_mode_->GetSelection() == 1,
                          static_cast<audiohole::ScanScope>(scan_scope_->GetSelection()),
                          device_id_);
        refresh();
    });
    scan_cancel_ = button(page, row, "Cancel scan", [this] {
        controller_->scan(false);
        refresh();
    });
    root->Add(row);
    scan_progress_ = new wxGauge(page, wxID_ANY, 100);
    root->Add(scan_progress_, 0, wxEXPAND | wxALL, 4);
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
            wxS("AudioHole GUI testing build — SIMULATED data; no hardware measurements.\n\n") +
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
        refresh();
        present(presentation::Message::DiagnosticsCleared);
    });
    root->Add(export_row);
    page->SetSizer(root);
    notebook_->AddPage(page, "Scanning / Diagnostics");
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
            wxS("AudioHole GUI testing build — SIMULATED data; no hardware measurements.\n\n") +
                console_->GetValue(),
            wxConvUTF8) ||
        !output.Close())
        show_result({false, "Could not write diagnostics to the selected file. Check permissions "
                            "and available space."});
    else
        present(presentation::Message::DiagnosticsExported);
}

} // namespace audiohole::gui
