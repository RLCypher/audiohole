#include "shortcut_icon.hpp"
#include "native_runtime.hpp"
#include <algorithm>
#include <limits>
#include <propkey.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace audiohole::plug_board::windows_detail {
namespace {
std::filesystem::path known_folder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    const auto status = SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &raw);
    const auto path =
        SUCCEEDED(status) && raw ? std::filesystem::path(raw) : std::filesystem::path{};
    CoTaskMemFree(raw);
    return path;
}
bool same_identity(const std::string& first, const std::string& second) {
    return !first.empty() && !second.empty() &&
           CompareStringOrdinal(wide(first).c_str(), -1, wide(second).c_str(), -1, TRUE) ==
               CSTR_EQUAL;
}
bool local_image(const std::wstring& value) {
    const auto offset = value.starts_with(L"\\\\?\\") ? 4u : 0u;
    return value.size() > offset + 3 &&
           ((value[offset] >= L'A' && value[offset] <= L'Z') ||
            (value[offset] >= L'a' && value[offset] <= L'z')) &&
           value[offset + 1] == L':' && (value[offset + 2] == L'\\' || value[offset + 2] == L'/');
}
std::string shortcut_app_id(IShellLinkW* link) {
    ComPointer<IPropertyStore> properties;
    if (FAILED(link->QueryInterface(IID_IPropertyStore, properties.put_void())))
        return {};
    struct Value {
        PROPVARIANT data{};
        ~Value() {
            PropVariantClear(&data);
        }
    } value;
    if (FAILED(properties->GetValue(PKEY_AppUserModel_ID, &value.data)) ||
        value.data.vt != VT_LPWSTR || !value.data.pwszVal)
        return {};
    return utf8(value.data.pwszVal);
}
} // namespace
std::vector<ShortcutIconEntry>
read_shortcut_icons(const std::vector<std::filesystem::path>& supplied) {
    std::vector<std::filesystem::path> roots = supplied;
    if (roots.empty()) {
        const auto pinned = known_folder(FOLDERID_UserPinned);
        roots = {pinned.empty() ? pinned : pinned / L"TaskBar", known_folder(FOLDERID_Programs),
                 known_folder(FOLDERID_CommonPrograms)};
    }
    std::vector<ShortcutIconEntry> entries;
    unsigned visited = 0, links = 0;
    // Bound traversal and COM loads independently, and never follow junctions or
    // symlinks into another volume/network. The caller caches this index until
    // explicit Refresh/reopen; it is not rebuilt for each row or on an idle timer.
    for (unsigned priority = 0; priority < roots.size() && visited < 4096 && links < 512;
         ++priority) {
        if (roots[priority].empty() || !local_image(roots[priority].wstring()))
            continue;
        std::error_code error;
        std::filesystem::recursive_directory_iterator iterator(
            roots[priority], std::filesystem::directory_options::skip_permission_denied, error),
            end;
        while (!error && iterator != end && ++visited <= 4096 && links < 512) {
            const auto path = iterator->path();
            const auto attributes = GetFileAttributesW(path.c_str());
            if (iterator.depth() >= 6 || (attributes != INVALID_FILE_ATTRIBUTES &&
                                          (attributes & FILE_ATTRIBUTE_REPARSE_POINT)))
                iterator.disable_recursion_pending();
            if (attributes != INVALID_FILE_ATTRIBUTES &&
                !(attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) &&
                same_identity(utf8(path.extension().wstring()), ".lnk")) {
                ++links;
                try {
                    ComPointer<IShellLinkW> link;
                    require(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                             IID_IShellLinkW, link.put_void()),
                            "Read shortcut icon metadata");
                    ComPointer<IPersistFile> file;
                    require(link->QueryInterface(IID_IPersistFile, file.put_void()),
                            "Read shortcut file");
                    require(file->Load(path.c_str(), STGM_READ), "Load local shortcut");
                    std::wstring icon(32768, L'\0');
                    int index = 0;
                    require(
                        link->GetIconLocation(icon.data(), static_cast<int>(icon.size()), &index),
                        "Read explicit shortcut icon");
                    icon.resize(wcslen(icon.c_str()));
                    if (!icon.empty()) {
                        std::wstring target(32768, L'\0');
                        link->GetPath(target.data(), static_cast<int>(target.size()), nullptr,
                                      SLGP_RAWPATH);
                        target.resize(wcslen(target.c_str()));
                        const auto identity =
                            local_image(target)
                                ? canonical_file_identity({IdentityKind::Win32Image, utf8(target)})
                                      .value
                                : std::string{};
                        entries.push_back(
                            {utf8(path.wstring()), identity, shortcut_app_id(link.get()),
                             utf8(L"\"" + icon + L"\"," + std::to_wstring(index)), priority});
                    }
                } catch (const std::exception&) { /* Invalid/unreadable links cannot become identity
                                                     evidence. */
                }
            }
            iterator.increment(error);
        }
    }
    return entries;
}
ShortcutIconSnapshot observe_shortcut_icon(const ApplicationObservation& app,
                                           const WindowIconSnapshot& window,
                                           const std::vector<ShortcutIconEntry>& entries) {
    ShortcutIconSnapshot result;
    std::vector<std::string> images;
    if (app.identity.kind == IdentityKind::Win32Image)
        images.push_back(canonical_file_identity(app.identity).value);
    for (const auto& token : app.verified_processes)
        images.push_back(
            canonical_file_identity({IdentityKind::Win32Image, token.executable_path}).value);
    const auto app_id = !window.app_user_model_id.empty() ? window.app_user_model_id
                        : app.identity.kind == IdentityKind::PackagedApplication
                            ? app.identity.value
                            : std::string{};
    unsigned best = std::numeric_limits<unsigned>::max();
    bool ambiguous = false;
    for (const auto& entry : entries) {
        const bool app_match = !app_id.empty() && entry.app_id == app_id;
        const bool image_match = std::any_of(images.begin(), images.end(), [&](const auto& image) {
            return same_identity(image, entry.target);
        });
        if (!app_match && !image_match)
            continue;
        const auto score = entry.priority * 2 + (app_match ? 0u : 1u);
        if (score < best) {
            best = score;
            ambiguous = false;
            result = {entry.resource, entry.path,
                      app_match ? "verified_app_id" : "verified_full_image"};
        } else if (score == best && entry.resource != result.resource)
            ambiguous = true;
    }
    if (ambiguous)
        return {{}, {}, "ambiguous_custom_icons"};
    return result;
}
} // namespace audiohole::plug_board::windows_detail
