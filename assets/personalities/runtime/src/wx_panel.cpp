#include "audiohole/avatar/wx_panel.hpp"
#include <algorithm>
#include <chrono>
#include <wx/dcbuffer.h>
#include <wx/image.h>

namespace audiohole::avatar {
std::int64_t monotonic_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
Panel::Panel(wxWindow* parent) : wxPanel(parent) {
    SetName("AudioHole avatar");
    SetMinSize(FromDIP(wxSize(144, 216)));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        update_bitmap();
        event.Skip();
    });
    Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
        dc.Clear();
        const auto size = GetClientSize();
        if (checkerboard_) {
            const int edge = FromDIP(16);
            dc.SetPen(*wxTRANSPARENT_PEN);
            for (int y = 0; y < size.y; y += edge)
                for (int x = 0; x < size.x; x += edge) {
                    dc.SetBrush(wxBrush((x / edge + y / edge) % 2 ? wxColour(195, 199, 207)
                                                                  : wxColour(231, 233, 239)));
                    dc.DrawRectangle(x, y, edge, edge);
                }
        }
        if (bitmap_.IsOk())
            dc.DrawBitmap(bitmap_, (size.x - bitmap_.GetWidth()) / 2,
                          (size.y - bitmap_.GetHeight()) / 2, true);
    });
    Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) {
        cached_bounds_ = {};
        update_bitmap();
        event.Skip();
    });
}
void Panel::present(Presentation value) {
    presentation_ = std::move(value);
    update_bitmap();
}
void Panel::fit_height(int physical_height) {
    if (physical_height <= 0)
        return;
    SetMinSize(wxSize((physical_height * 512 + 767) / 768, physical_height));
}
void Panel::update_bitmap() {
    const auto size = GetClientSize();
    if (!presentation_.image) {
        if (bitmap_.IsOk()) {
            bitmap_ = wxBitmap();
            cached_digest_.clear();
            bitmap_bytes_ = 0;
            Refresh();
        }
        return;
    }
    const auto& pixels = *presentation_.image;
    if (size.x <= 0 || size.y <= 0 || (size == cached_bounds_ && cached_digest_ == pixels.digest))
        return;
    // One scaled bitmap only. Repaints never decode files or rebuild this cache.
    wxImage image(pixels.width, pixels.height);
    image.InitAlpha();
    for (std::size_t p = 0; p < pixels.rgba.size() / 4; ++p) {
        std::copy_n(pixels.rgba.data() + p * 4, 3, image.GetData() + p * 3);
        image.GetAlpha()[p] = pixels.rgba[p * 4 + 3];
    }
    const double scale = std::min({double(size.x) / 512, double(size.y) / 768, 4.0});
    const int width = std::max(1, int(512 * scale)), height = std::max(1, int(768 * scale));
    if (width != 512 || height != 768) {
        // Area filtering keeps compact line art readable. Bilinear upscaling
        // avoids a costly bicubic software pass when a viewer is enlarged.
        const auto quality = scale > 1 ? wxIMAGE_QUALITY_BILINEAR : wxIMAGE_QUALITY_HIGH;
        image = image.Scale(width, height, quality);
    }
    bitmap_ = wxBitmap(image);
    bitmap_bytes_ = static_cast<std::size_t>(width) * height * 4;
    cached_digest_ = pixels.digest;
    cached_bounds_ = size;
    Refresh();
}
} // namespace audiohole::avatar
