#pragma once
#include "presentation/icon_outline.h"
#include <filesystem>
#include <map>
#include <tuple>
#include <wx/iconbndl.h>
#include <wx/image.h>
#include <wx/panel.h>
#include <wx/wx.h>

namespace audiohole::gui {
// Decodes the embedded original once, using Windows' system PNG codec. Borrowed
// immutable image remains valid for the process. GUI-thread/initialized COM only.
const wxImage& canonical_logo();
const wxImage& placeholder_avatar();
wxImage package_preview(const std::filesystem::path&);
wxIconBundle application_icons();
// Pure view rendering: scales once per size/state, then strokes the exterior.
// Cache owns native icons until the tray is destroyed; no audio-thread access.
class TrayIconCache {
  public:
    const wxIcon& get(int physical_size, audiohole::State state);
    std::size_t size() const {
        return icons_.size();
    }

  private:
    std::map<std::tuple<int, int, int, int>, wxIcon> icons_;
};
// Static branding/legacy preview renderer; animation uses avatar::Panel.
// wx owns this child window. The image is copied by
// wx ref-counting; callers retain their own image. No personality/engine commands,
// animation, text generation, or I/O occurs during paint. All calls are GUI-thread.
class AgentDisplay final : public wxPanel {
  public:
    explicit AgentDisplay(wxWindow* parent, wxSize logical_size = wxSize(128, 128));
    // Reserve the source's proportional width for this physical header height.
    // The parent sizer allocates bounds; neither source pixels nor alpha change.
    void fit_height(int physical_height);
    void set_image(const wxImage& image);
    void clear_image();
    bool has_image() const {
        return bitmap_.IsOk();
    }

  private:
    wxSize logical_size_;
    wxImage source_;
    wxBitmap bitmap_;
    wxSize cached_bounds_;
    void update_bitmap();
};
} // namespace audiohole::gui
