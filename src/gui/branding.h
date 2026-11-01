#pragma once
#include "presentation/icon_outline.h"
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
// Reserved for a future agent. wx owns this child window. The image is copied by
// wx ref-counting; callers retain their own image. No personality/engine commands,
// animation, text generation, or I/O occurs during paint. All calls are GUI-thread.
class AgentDisplay final : public wxPanel {
  public:
    explicit AgentDisplay(wxWindow* parent);
    void set_image(const wxImage& image);
    bool has_image() const {
        return bitmap_.IsOk();
    }

  private:
    wxImage source_;
    wxBitmap bitmap_;
    int cached_size_ = 0;
    void update_bitmap();
};
} // namespace audiohole::gui
