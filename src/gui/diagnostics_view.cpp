#include "gui/controls.h"
#include "gui/frame.h"

namespace audiohole::gui {
void Frame::build_scan() {
    auto* page = new wxPanel(notebook_);
    auto* root = new wxBoxSizer(wxVERTICAL);
    label(page, root,
          "Scanning and diagnostics — SIMULATED. No real enumeration, stream initialization, or "
          "intrusive probing.");
    auto* options = new wxBoxSizer(wxHORIZONTAL);
    scan_mode_ = choices(page, options, "Testing level",
                         {"Easy (fixture-reported)", "Advanced (simulated intrusive consent)"});
    scan_scope_ =
        choices(page, options, "Scope", {"Update saved devices", "Full scan", "Selected device"});
    root->Add(options, 0, wxEXPAND);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    scan_start_ = button(page, row, "Start simulated scan", [this] {
        if (scan_mode_->GetSelection() == 1 &&
            wxMessageBox("Production advanced probing may interrupt devices or produce sound. This "
                         "simulation opens no streams. Continue simulated advanced scan?",
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
    label(page, root,
          "Experimental capability use is separate from testing consent and unavailable here. "
          "Fixture data is never promoted to hardware verification.");
    severity_ = choices(page, root, "Diagnostic filter",
                        {"All events", "Errors only", "Routing state only"});
    log_search_ = field(page, root, "Search technical diagnostics");
    console_ = new wxTextCtrl(page, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    root->Add(console_, 1, wxEXPAND | wxALL, 4);
    auto* export_row = new wxBoxSizer(wxHORIZONTAL);
    button(page, export_row, "Copy visible diagnostics", [this] {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(console_->GetValue()));
            wxTheClipboard->Close();
        }
    });
    button(page, export_row, "Export visible diagnostics…", [this] { export_log(); });
    root->Add(export_row);
    label(page, root,
          "Bounded to the most recent 500 technical events. No audio samples, commentary, or user "
          "notes are exported.");
    page->SetSizer(root);
    notebook_->AddPage(page, "Scanning / Diagnostics");
}
void Frame::export_log() {
    wxFileDialog dialog(this, "Export simulated technical diagnostics", {},
                        "AudioHole-simulated-diagnostics.txt", "Text files (*.txt)|*.txt",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() == wxID_OK && !console_->SaveFile(dialog.GetPath()))
        show_result({false, "Could not write diagnostics to the selected file. Check permissions "
                            "and available space."});
}

} // namespace audiohole::gui
