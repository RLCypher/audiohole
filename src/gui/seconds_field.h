#pragma once
#include <optional>
#include <wx/spinbutt.h>
#include <wx/wx.h>
namespace audiohole::gui {
// GUI-thread control with exact decimal seconds and 1 ms arrow increments.
// Bounds and initial value use backend milliseconds; invalid text stays visible
// and returns nullopt so the request can report it without silently rounding.
class SecondsField final : public wxPanel {
  public:
    SecondsField(wxWindow* parent, unsigned minimum_ms, unsigned maximum_ms, unsigned initial_ms);
    std::optional<unsigned> milliseconds() const;
    wxTextCtrl* editor() const {
        return text_;
    }

  private:
    wxTextCtrl* text_;
    unsigned minimum_, maximum_;
    void nudge(int milliseconds);
};
} // namespace audiohole::gui
