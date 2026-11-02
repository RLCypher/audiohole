#include "audiohole/avatar/assets.hpp"
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <shlwapi.h>
#include <wincodec.h>
#include <windows.h>
#else
#include <wx/image.h>
#include <wx/imagpng.h>
#include <wx/log.h>
#include <wx/mstream.h>
#endif

namespace audiohole::avatar {
#ifdef _WIN32
namespace {
struct ReleaseCom {
    void operator()(IUnknown* value) const {
        if (value)
            value->Release();
    }
};
template <class T> using ComPtr = std::unique_ptr<T, ReleaseCom>;
void check(HRESULT result) {
    if (FAILED(result))
        throw std::runtime_error("WIC decode failed: " + std::to_string(result));
}
struct Apartment {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Apartment() {
        if (result != RPC_E_CHANGED_MODE)
            check(result);
    }
    ~Apartment() {
        if (SUCCEEDED(result))
            CoUninitialize();
    }
};
Pixels decode(std::span<const std::uint8_t> bytes) {
    Apartment apartment;
    ComPtr<IStream> stream(SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
    if (!stream)
        throw std::runtime_error("Cannot create WIC input stream");
    IWICImagingFactory* raw_factory = nullptr;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_IWICImagingFactory, reinterpret_cast<void**>(&raw_factory)));
    ComPtr<IWICImagingFactory> factory(raw_factory);
    IWICBitmapDecoder* raw_decoder = nullptr;
    check(factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnLoad,
                                           &raw_decoder));
    ComPtr<IWICBitmapDecoder> decoder(raw_decoder);
    UINT count = 0;
    check(decoder->GetFrameCount(&count));
    if (count != 1)
        throw std::runtime_error("Operating PNG contains multiple frames");
    IWICBitmapFrameDecode* raw_frame = nullptr;
    check(decoder->GetFrame(0, &raw_frame));
    ComPtr<IWICBitmapFrameDecode> frame(raw_frame);
    UINT width = 0, height = 0;
    check(frame->GetSize(&width, &height));
    if (width != 512 || height != 768)
        throw std::runtime_error("WIC dimensions outside contract");
    IWICFormatConverter* raw_converter = nullptr;
    check(factory->CreateFormatConverter(&raw_converter));
    ComPtr<IWICFormatConverter> converter(raw_converter);
    check(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                nullptr, 0, WICBitmapPaletteTypeCustom));
    Pixels result;
    result.rgba.resize(512 * 768 * 4);
    check(converter->CopyPixels(nullptr, 512 * 4, static_cast<UINT>(result.rgba.size()),
                                result.rgba.data()));
    return result;
}
} // namespace
void initialize_image_decoder() {}
Decoder native_decoder() {
    return decode;
}
#else
void initialize_image_decoder() {
    if (!wxImage::FindHandler(wxBITMAP_TYPE_PNG))
        wxImage::AddHandler(new wxPNGHandler);
}
Decoder native_decoder() {
    return [](std::span<const std::uint8_t> bytes) {
        wxLogNull suppress_codec_dialogs;
        wxMemoryInputStream stream(bytes.data(), bytes.size());
        wxImage image(stream, wxBITMAP_TYPE_PNG);
        if (!image.IsOk() || image.GetWidth() != 512 || image.GetHeight() != 768 ||
            !image.HasAlpha())
            throw std::runtime_error("PNG codec rejected image dimensions/alpha");
        Pixels pixels;
        pixels.rgba.resize(512 * 768 * 4);
        for (std::size_t p = 0; p < 512 * 768; ++p) {
            std::copy_n(image.GetData() + p * 3, 3, pixels.rgba.data() + p * 4);
            pixels.rgba[p * 4 + 3] = image.GetAlpha()[p];
        }
        return pixels;
    };
}
#endif
} // namespace audiohole::avatar
