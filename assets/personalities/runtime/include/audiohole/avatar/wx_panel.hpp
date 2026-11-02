#pragma once
#include "audiohole/avatar/session.hpp"
#include <wx/panel.h>
#include <wx/timer.h>

namespace audiohole::avatar {
// Parent-owned native wx widget, usable by AudioHole and the standalone viewer.
// Only drawing/scaling occurs on the GUI thread. Playback belongs to Session.
class Panel final : public wxPanel {
  public:
    explicit Panel(wxWindow* parent);
    void present(Presentation presentation);
    void fit_height(int physical_height);
    void checkerboard(bool value) {
        checkerboard_ = value;
        Refresh();
    }
    const Presentation& presentation() const {
        return presentation_;
    }
    std::size_t bitmap_bytes() const {
        return bitmap_bytes_;
    }

  private:
    Presentation presentation_;
    wxBitmap bitmap_;
    std::string cached_digest_;
    wxSize cached_bounds_;
    bool checkerboard_ = true;
    std::size_t bitmap_bytes_ = 0;
    void update_bitmap();
};
std::int64_t monotonic_ms();
} // namespace audiohole::avatar
