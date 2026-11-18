#include "audiohole/plug_board/windows.hpp"
#include "icon_path.hpp"
#include "native_runtime.hpp"
#include "support.hpp"

#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <wincodec.h>

#include <exception>
#include <map>
#include <thread>

namespace audiohole::plug_board {
using namespace windows_detail;
namespace {

// Resource ownership differs: GetImage returns an owned HBITMAP, resource
// extraction returns an owned HICON, LoadIcon's built-in fallback is shared.
class Bitmap {
  public:
    ~Bitmap() {
        if (value) {
            DeleteObject(value);
        }
    }
    HBITMAP value = nullptr;
};
class Icon {
  public:
    ~Icon() {
        if (value) {
            DestroyIcon(value);
        }
    }
    HICON value = nullptr;
};

// PNG conversion runs entirely on the Shell worker. The chosen encoder format
// can differ from the source bitmap; an explicit WIC converter preserves alpha.
void write_png(IWICImagingFactory* factory, IWICBitmapSource* bitmap,
               const std::filesystem::path& file) {
    if (std::filesystem::exists(file)) {
        throw std::runtime_error("Icon evidence path already exists; no overwrite permitted.");
    }
    ComPointer<IWICStream> stream;
    require(factory->CreateStream(stream.put()), "Create icon stream");
    require(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE),
            "Create PNG evidence file");
    ComPointer<IWICBitmapEncoder> encoder;
    require(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()),
            "Create PNG encoder");
    require(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache), "Initialize PNG encoder");
    ComPointer<IWICBitmapFrameEncode> frame;
    require(encoder->CreateNewFrame(frame.put(), nullptr), "Create PNG frame");
    require(frame->Initialize(nullptr), "Initialize PNG frame");
    UINT width = 0, height = 0;
    require(bitmap->GetSize(&width, &height), "Get actual icon size");
    require(frame->SetSize(width, height), "Set PNG dimensions");
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    require(frame->SetPixelFormat(&format), "Set PNG pixel format");
    ComPointer<IWICFormatConverter> converter;
    require(factory->CreateFormatConverter(converter.put()), "Create icon format converter");
    require(converter->Initialize(bitmap, format, WICBitmapDitherTypeNone, nullptr, 0,
                                  WICBitmapPaletteTypeCustom),
            "Convert icon pixels");
    require(frame->WriteSource(converter.get(), nullptr), "Write icon pixels");
    require(frame->Commit(), "Commit PNG frame");
    require(encoder->Commit(), "Commit PNG evidence");
}

// A packaged application uses AppsFolder, not its executable host's icon. For an
// unresolved application only an already-verified host path can supply a display
// icon; it still does not become a durable identity for the hosted application.
std::string verified_icon_image(const ApplicationObservation& application) {
    if (application.identity.kind == IdentityKind::Win32Image) {
        return application.identity.value;
    }
    if (application.identity.kind == IdentityKind::Unresolved) {
        for (const auto& session : application.sessions) {
            if (session.process && !session.process->executable_path.empty()) {
                return session.process->executable_path;
            }
        }
    }
    return {};
}

ComPointer<IWICBitmap> resolve_shell_icon(IWICImagingFactory* factory,
                                          const ApplicationObservation& application,
                                          unsigned pixels) {
    ComPointer<IShellItemImageFactory> shell;
    HRESULT result = E_FAIL;
    if (application.identity.kind == IdentityKind::PackagedApplication) {
        result = SHCreateItemInKnownFolder(FOLDERID_AppsFolder, 0,
                                           wide(application.identity.value).c_str(),
                                           IID_IShellItemImageFactory, shell.put_void());
    } else {
        const auto image = verified_icon_image(application);
        if (!image.empty()) {
            const auto parsing_name = shell_image_path(wide(image));
            result = SHCreateItemFromParsingName(parsing_name.c_str(), nullptr,
                                                 IID_IShellItemImageFactory, shell.put_void());
        }
    }
    require(result, "Resolve verified application Shell item");
    Bitmap bitmap;
    const SIZE size{static_cast<LONG>(pixels), static_cast<LONG>(pixels)};
    require(shell->GetImage(size, SIIGBF_ICONONLY, &bitmap.value),
            "Extract actual Shell application icon");
    ComPointer<IWICBitmap> converted;
    require(factory->CreateBitmapFromHBITMAP(bitmap.value, nullptr, WICBitmapUsePremultipliedAlpha,
                                             converted.put()),
            "Read Shell icon pixels");
    return converted;
}

// This lower-level entry point treats a verified filename literally. In
// particular, a '%' or comma in an executable filename is not an environment
// reference or an icon ordinal. The session metadata parser below is separate.
ComPointer<IWICBitmap> extract_file_icon(IWICImagingFactory* factory, const std::wstring& file,
                                         int ordinal, unsigned pixels) {
    Icon icon;
    require(
        SHDefExtractIconW(file.c_str(), ordinal, 0, &icon.value, nullptr, MAKELONG(pixels, pixels)),
        "Extract actual executable/resource icon");
    ComPointer<IWICBitmap> bitmap;
    require(factory->CreateBitmapFromHICON(icon.value, bitmap.put()), "Read resource icon pixels");
    return bitmap;
}

ComPointer<IWICBitmap> resolve_resource_icon(IWICImagingFactory* factory, const std::string& source,
                                             unsigned pixels) {
    auto path = wide(source);
    if (!path.empty() && path.front() == L'@') {
        path.erase(path.begin());
    }
    std::wstring expanded(32768, L'\0');
    const auto length = ExpandEnvironmentStringsW(path.c_str(), expanded.data(),
                                                  static_cast<DWORD>(expanded.size()));
    if (!length || length > expanded.size()) {
        throw std::runtime_error("Application icon resource path could not be expanded.");
    }
    expanded.resize(length - 1);
    const int ordinal = PathParseIconLocationW(expanded.data());
    expanded.resize(wcslen(expanded.c_str()));
    // Restrict session-provided fallbacks to local absolute files. A metadata
    // string must not make Shell fetch a URL or an arbitrary remote resource.
    const bool extended_local =
        expanded.starts_with(L"\\\\?\\") && expanded.size() > 6 && expanded[5] == L':';
    if (!std::filesystem::path(expanded).is_absolute() ||
        (expanded.starts_with(L"\\\\") && !extended_local)) {
        throw std::runtime_error("Icon fallback is not an absolute local resource.");
    }
    return extract_file_icon(factory, shell_image_path(expanded), ordinal, pixels);
}

// Copy decoded pixels while the Shell worker already owns the bitmap. The GUI
// receives plain values and never needs to reopen a PNG or run a Shell decoder.
void copy_rgba(IWICImagingFactory* factory, IWICBitmapSource* source, IconExport& exported) {
    UINT width = 0, height = 0;
    require(source->GetSize(&width, &height), "Read icon dimensions");
    if (!width || !height || width > 256 || height > 256) {
        throw std::runtime_error("Icon dimensions exceed the bounded GUI image size.");
    }
    ComPointer<IWICFormatConverter> converter;
    require(factory->CreateFormatConverter(converter.put()), "Create GUI icon converter");
    require(converter->Initialize(source, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                  nullptr, 0, WICBitmapPaletteTypeCustom),
            "Decode GUI icon pixels");
    exported.rgba.resize(width * height * 4);
    require(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(exported.rgba.size()),
                                  exported.rgba.data()),
            "Copy GUI icon pixels");
    exported.width = width;
    exported.height = height;
}

std::vector<IconExport> export_batch(const std::vector<ApplicationObservation>& applications,
                                     const std::filesystem::path& directory, unsigned pixels,
                                     std::stop_token stop,
                                     const WindowIconSnapshot* observed_window = nullptr,
                                     const ShortcutIconSnapshot* observed_shortcut = nullptr) {
    Apartment apartment(true);
    require(apartment.result(), "Initialize Shell icon STA");
    ComPointer<IWICImagingFactory> factory;
    require(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                             IID_IWICImagingFactory, factory.put_void()),
            "Create Windows image factory");
    const bool save_evidence = !directory.empty();
    if (save_evidence)
        std::filesystem::create_directories(directory);
    std::map<std::string, IconExport> cache;
    std::vector<IconExport> exports;
    const auto shortcut_index =
        observed_shortcut ? std::vector<ShortcutIconEntry>{} : read_shortcut_icons();
    for (const auto& application : applications) {
        if (stop.stop_requested()) {
            break;
        }
        if (const auto cached = cache.find(application.observation_key); cached != cache.end()) {
            exports.push_back(cached->second);
            continue;
        }
        IconExport exported;
        exported.observation_key = application.observation_key;
        if (save_evidence)
            exported.file = directory / (L"application-" + std::to_wstring(cache.size()) + L".png");
        ComPointer<IWICBitmap> bitmap;
        std::string primary_error;
        WindowIconSnapshot window;
        if (observed_window)
            window = *observed_window;
        else {
            try {
                window = observe_window_icon(application, pixels);
            } catch (...) { /* Shell fallback remains available. */
            }
        }
        const auto shortcut = observed_shortcut
                                  ? *observed_shortcut
                                  : observe_shortcut_icon(application, window, shortcut_index);
        exported.window_icon_kind = window.icon_kind;
        exported.taskbar_app_user_model_id = window.app_user_model_id;
        exported.shortcut_match = shortcut.match;
        exported.shortcut_path = shortcut.path;
        // Taskbar-specific metadata and WM_SETICON/WM_GETICON take priority over
        // the executable resource (which may be a generic game-engine logo).
        if (!window.resource.empty()) {
            try {
                bitmap = resolve_resource_icon(factory.get(), window.resource, pixels);
                exported.source = "taskbar_relaunch_resource";
            } catch (const std::exception& error) {
                primary_error = error.what();
            }
        }
        if (!bitmap && !shortcut.resource.empty()) {
            try {
                bitmap = resolve_resource_icon(factory.get(), shortcut.resource, pixels);
                exported.source = "taskbar_shortcut_icon";
            } catch (const std::exception& error) {
                primary_error += error.what();
            }
        }
        const auto taskbar_app_id = !window.app_user_model_id.empty() ? window.app_user_model_id
                                    : application.identity.kind == IdentityKind::PackagedApplication
                                        ? application.identity.value
                                        : std::string{};
        if (!bitmap && !taskbar_app_id.empty()) {
            try {
                auto taskbar = application;
                taskbar.identity = {IdentityKind::PackagedApplication, taskbar_app_id};
                bitmap = resolve_shell_icon(factory.get(), taskbar, pixels);
                exported.source = "taskbar_application_shell_icon";
            } catch (const std::exception& error) {
                primary_error += error.what();
            }
        }
        auto window_pixels = [&] {
            require(
                factory->CreateBitmapFromHICON(static_cast<HICON>(window.icon.get()), bitmap.put()),
                "Read taskbar window icon pixels");
            exported.source =
                window.class_fallback ? "taskbar_window_class_icon" : "taskbar_window_icon";
        };
        if (!bitmap && window.icon && !window.class_fallback) {
            try {
                window_pixels();
            } catch (const std::exception& error) {
                primary_error += error.what();
            }
        }
        if (!bitmap && window.icon) {
            try {
                window_pixels();
            } catch (const std::exception& error) {
                primary_error += error.what();
            }
        }
        if (bitmap)
            exported.application_specific = true;
        try {
            if (!bitmap) {
                bitmap = resolve_shell_icon(factory.get(), application, pixels);
                exported.source = "verified_application_shell_icon";
                exported.application_specific = true;
            }
        } catch (const std::exception& error) {
            primary_error = error.what();
        }
        if (!bitmap) {
            for (const auto& session : application.sessions) {
                if (session.icon_path.empty()) {
                    continue;
                }
                try {
                    bitmap = resolve_resource_icon(factory.get(), session.icon_path, pixels);
                    exported.source = "session_application_resource";
                    exported.application_specific = true;
                    break;
                } catch (const std::exception& error) {
                    primary_error += " " + std::string(error.what());
                }
            }
        }
        if (!bitmap) {
            const auto image = verified_icon_image(application);
            if (!image.empty()) {
                try {
                    // Most sessions provide no icon_path. Recover the real
                    // executable resource before falling back to a stock icon.
                    bitmap =
                        extract_file_icon(factory.get(), shell_image_path(wide(image)), 0, pixels);
                    exported.source = "verified_executable_resource";
                    exported.application_specific = true;
                } catch (const std::exception& error) {
                    primary_error += " " + std::string(error.what());
                }
            }
        }
        try {
            if (!bitmap) {
                // This fallback is explicitly labelled; its successful export
                // must not count as successful application-specific resolution.
                const auto shared_icon = LoadIconW(nullptr, IDI_APPLICATION);
                require(factory->CreateBitmapFromHICON(shared_icon, bitmap.put()),
                        "Load generic fallback icon");
                exported.source = "generic_fallback";
            }
            // A game can supply a 256/512 px window icon even at normal DPI.
            // Resample on the worker before enforcing the bounded RGBA contract.
            UINT width = 0, height = 0;
            require(bitmap->GetSize(&width, &height), "Read selected icon dimensions");
            if (!width || !height || width > 2048 || height > 2048)
                throw std::runtime_error("Window icon exceeds its decode bound.");
            if (width != pixels || height != pixels) {
                ComPointer<IWICBitmapScaler> scaler;
                require(factory->CreateBitmapScaler(scaler.put()), "Create taskbar icon scaler");
                require(scaler->Initialize(bitmap.get(), pixels, pixels,
                                           WICBitmapInterpolationModeFant),
                        "Scale taskbar icon");
                ComPointer<IWICBitmap> scaled;
                require(factory->CreateBitmapFromSource(scaler.get(), WICBitmapCacheOnLoad,
                                                        scaled.put()),
                        "Copy scaled icon");
                bitmap = std::move(scaled);
            }
            copy_rgba(factory.get(), bitmap.get(), exported);
            if (save_evidence)
                write_png(factory.get(), bitmap.get(), exported.file);
            exported.result = {
                true, exported.application_specific ? "application_icon" : "generic_fallback",
                primary_error};
        } catch (const std::exception& error) {
            exported.result = {false, "icon_export_failed", error.what()};
        }
        cache.emplace(application.observation_key, exported);
        exports.push_back(std::move(exported));
    }
    return exports;
}

} // namespace

ApplicationImage windows_detail::resolve_icon_pixels(const ApplicationObservation& application,
                                                     unsigned pixels,
                                                     const WindowIconSnapshot* window,
                                                     const ShortcutIconSnapshot* shortcut) {
    // The live cache shares the exact native resolver with the evidence exporter,
    // but an empty internal destination forbids directory and PNG writes.
    auto batch = export_batch({application}, {}, pixels, {}, window, shortcut);
    auto& icon = batch.front();
    return {icon.width,
            icon.height,
            std::move(icon.rgba),
            icon.application_specific,
            std::move(icon.source),
            std::move(icon.result),
            {{"window_icon_kind", icon.window_icon_kind},
             {"shortcut_match", icon.shortcut_match},
             {"shortcut_path", icon.shortcut_path},
             {"app_user_model_id", icon.taskbar_app_user_model_id},
             {"resolution_revision", 4}}};
}

ApplicationImage windows_detail::scale_icon_pixels(ApplicationImage image, unsigned pixels) {
    if (image.width == pixels && image.height == pixels)
        return image;
    if (pixels < 16 || pixels > 256 || !image.width || image.width > 256 || !image.height ||
        image.height > 256 || image.rgba.size() != image.width * image.height * 4)
        throw std::runtime_error("Invalid retained icon dimensions.");
    ComPointer<IWICImagingFactory> factory;
    require(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                             IID_IWICImagingFactory, factory.put_void()),
            "Create retained-icon image factory");
    ComPointer<IWICBitmap> bitmap;
    require(factory->CreateBitmapFromMemory(image.width, image.height, GUID_WICPixelFormat32bppRGBA,
                                            image.width * 4, static_cast<UINT>(image.rgba.size()),
                                            image.rgba.data(), bitmap.put()),
            "Read retained taskbar pixels");
    ComPointer<IWICBitmapScaler> scaler;
    require(factory->CreateBitmapScaler(scaler.put()), "Create retained-icon scaler");
    require(scaler->Initialize(bitmap.get(), pixels, pixels, WICBitmapInterpolationModeFant),
            "Scale retained taskbar pixels");
    IconExport scaled;
    copy_rgba(factory.get(), scaler.get(), scaled);
    image.width = scaled.width;
    image.height = scaled.height;
    image.rgba = std::move(scaled.rgba);
    return image;
}

std::vector<IconExport>
export_windows_icons(const std::vector<ApplicationObservation>& applications,
                     const std::filesystem::path& directory, unsigned pixels,
                     std::stop_token stop) {
    if (directory.empty())
        throw std::runtime_error("An icon evidence directory is required.");
    if (pixels < 16 || pixels > 256) {
        throw std::runtime_error("Icon evidence size must be between 16 and 256 pixels.");
    }
    std::vector<IconExport> exports;
    std::exception_ptr failure;
    std::thread worker([&] {
        try {
            exports = export_batch(applications, directory, pixels, stop);
        } catch (...) {
            failure = std::current_exception();
        }
    });
    worker.join();
    if (failure) {
        std::rethrow_exception(failure);
    }
    return exports;
}

} // namespace audiohole::plug_board
