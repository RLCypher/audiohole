#include "gui/branding.h"
#include "gui/branding_resources.h"
#include <algorithm>
#include <memory>
#include <shlwapi.h>
#include <stdexcept>
#include <wincodec.h>
#include <wx/dcbuffer.h>
#include <wx/msw/wrapwin.h>

namespace audiohole::gui {
namespace {
// COM references are acquired/used/released on the existing GUI STA. WIC and
// shlwapi are Windows components, not redistributed third-party runtime codecs.
struct ReleaseCom {
    void operator()(IUnknown* object) const {
        if (object)
            object->Release();
    }
};
void check(HRESULT result) {
    if (FAILED(result))
        throw std::runtime_error("Branding image decode failed, HRESULT " + std::to_string(result));
}
wxImage decode_logo(int resource_id) {
    const auto module = GetModuleHandleW(nullptr);
    const auto resource = FindResourceW(module, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (!resource)
        throw std::runtime_error("Embedded AudioHole logo resource is missing");
    // Resource bytes are borrowed from the loaded executable for process lifetime;
    // SHCreateMemStream copies them into an independently owned decode stream.
    const auto bytes = static_cast<const BYTE*>(LockResource(LoadResource(module, resource)));
    const DWORD count = SizeofResource(module, resource);
    if (!bytes || !count)
        throw std::runtime_error("Embedded AudioHole logo resource is empty");
    std::unique_ptr<IStream, ReleaseCom> stream(SHCreateMemStream(bytes, count));
    if (!stream)
        throw std::runtime_error("Could not create logo decode stream");
    IWICImagingFactory* raw_factory = nullptr;
    auto result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_IWICImagingFactory, reinterpret_cast<void**>(&raw_factory));
    std::unique_ptr<IWICImagingFactory, ReleaseCom> factory(raw_factory);
    check(result);
    IWICBitmapDecoder* raw_decoder = nullptr;
    result = factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnLoad,
                                              &raw_decoder);
    std::unique_ptr<IWICBitmapDecoder, ReleaseCom> decoder(raw_decoder);
    check(result);
    IWICBitmapFrameDecode* raw_frame = nullptr;
    result = decoder->GetFrame(0, &raw_frame);
    std::unique_ptr<IWICBitmapFrameDecode, ReleaseCom> frame(raw_frame);
    check(result);
    UINT width = 0, height = 0;
    check(frame->GetSize(&width, &height));
    if (!width || !height || width > 4096 || height > 4096)
        throw std::runtime_error("Invalid embedded presentation image dimensions");
    IWICFormatConverter* raw_converter = nullptr;
    result = factory->CreateFormatConverter(&raw_converter);
    std::unique_ptr<IWICFormatConverter, ReleaseCom> converter(raw_converter);
    check(result);
    check(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                nullptr, 0, WICBitmapPaletteTypeCustom));
    std::vector<BYTE> rgba(width * height * 4);
    check(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(rgba.size()), rgba.data()));
    wxImage image(static_cast<int>(width), static_cast<int>(height));
    image.InitAlpha();
    for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
        std::copy_n(rgba.data() + pixel * 4, 3, image.GetData() + pixel * 3);
        image.GetAlpha()[pixel] = rgba[pixel * 4 + 3];
    }
    return image;
}
} // namespace
const wxImage& canonical_logo() {
    static const wxImage image = decode_logo(AUDIOHOLE_LOGO_PNG);
    return image;
}
const wxImage& placeholder_avatar() {
    static const wxImage image = decode_logo(AUDIOHOLE_PLACEHOLDER_PNG);
    return image;
}
wxIconBundle application_icons() {
    wxIconBundle icons;
    for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
        // No LR_SHARED: wxIcon takes ownership of each independently loaded HICON.
        const auto handle = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),
                                                          MAKEINTRESOURCEW(AUDIOHOLE_APP_ICON),
                                                          IMAGE_ICON, size, size, 0));
        if (!handle)
            throw std::runtime_error("Embedded application icon is missing");
        wxIcon icon;
        if (!icon.CreateFromHICON(handle))
            throw std::runtime_error("Could not wrap the application icon");
        icons.AddIcon(icon);
    }
    return icons;
}
const wxIcon& TrayIconCache::get(int physical_size, audiohole::State state) {
    // Bound cache dimensions even if a malformed DPI setting is reported.
    physical_size = std::clamp(physical_size, 16, 64);
    const auto colour = presentation::tray_colour(state);
    const auto key = std::make_tuple(physical_size, colour.red, colour.green, colour.blue);
    if (const auto found = icons_.find(key); found != icons_.end())
        return found->second;
    auto scaled = canonical_logo().Scale(physical_size, physical_size, wxIMAGE_QUALITY_HIGH);
    presentation::RgbaImage rgba{
        physical_size, physical_size,
        std::vector<std::uint8_t>(static_cast<std::size_t>(physical_size * physical_size * 4))};
    for (std::size_t pixel = 0; pixel < rgba.pixels.size() / 4; ++pixel) {
        std::copy_n(scaled.GetData() + pixel * 3, 3, rgba.pixels.data() + pixel * 4);
        rgba.pixels[pixel * 4 + 3] = scaled.GetAlpha()[pixel];
    }
    const auto outlined = presentation::outline_logo(rgba, colour, physical_size >= 40 ? 2 : 1);
    for (std::size_t pixel = 0; pixel < rgba.pixels.size() / 4; ++pixel) {
        std::copy_n(outlined.pixels.data() + pixel * 4, 3, scaled.GetData() + pixel * 3);
        scaled.GetAlpha()[pixel] = outlined.pixels[pixel * 4 + 3];
    }
    wxIcon icon;
    icon.CopyFromBitmap(wxBitmap(scaled));
    return icons_.emplace(key, icon).first->second;
}
AgentDisplay::AgentDisplay(wxWindow* parent, wxSize logical_size)
    : wxPanel(parent), logical_size_(logical_size) {
    SetMinSize(FromDIP(logical_size_));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetName("AudioHole agent display");
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        update_bitmap();
        event.Skip();
    });
    Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) {
        SetMinSize(FromDIP(logical_size_));
        cached_bounds_ = wxSize();
        update_bitmap();
        GetParent()->Layout();
        event.Skip();
    });
    Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        if (bitmap_.IsOk()) {
            const auto size = GetClientSize();
            dc.DrawBitmap(bitmap_, (size.x - bitmap_.GetWidth()) / 2,
                          (size.y - bitmap_.GetHeight()) / 2, true);
        }
    });
    set_image(canonical_logo());
}
void AgentDisplay::fit_height(int physical_height) {
    if (!source_.IsOk() || physical_height <= 0)
        return;
    // Round width upward so fitting the complete image can never reduce height
    // to satisfy a narrower region (the temporary art is wider than 2:3).
    const auto width =
        (static_cast<long long>(physical_height) * source_.GetWidth() + source_.GetHeight() - 1) /
        source_.GetHeight();
    const wxSize bounds(static_cast<int>(width), physical_height);
    if (GetMinSize() == bounds)
        return;
    logical_size_ = ToDIP(bounds);
    SetMinSize(bounds);
}
void AgentDisplay::set_image(const wxImage& image) {
    if (!image.IsOk())
        throw std::invalid_argument("Agent image must be valid");
    source_ = image;
    cached_bounds_ = wxSize();
    update_bitmap();
}
void AgentDisplay::update_bitmap() {
    const auto size = GetClientSize();
    const int edge = std::min(size.x, size.y);
    if (!source_.IsOk() || edge <= 0 || size == cached_bounds_)
        return;
    const double scale = std::min(static_cast<double>(size.x) / source_.GetWidth(),
                                  static_cast<double>(size.y) / source_.GetHeight());
    bitmap_ = wxBitmap(source_.Scale(std::max(1, static_cast<int>(source_.GetWidth() * scale)),
                                     std::max(1, static_cast<int>(source_.GetHeight() * scale)),
                                     wxIMAGE_QUALITY_HIGH));
    cached_bounds_ = size;
    Refresh();
}
} // namespace audiohole::gui
