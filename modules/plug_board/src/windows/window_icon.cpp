#include "window_icon.hpp"
#include "native_runtime.hpp"

#include <dwmapi.h>
#include <propkey.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <map>
#include <set>

namespace audiohole::plug_board::windows_detail {
namespace {
struct Candidate {
    HWND window{};
    DWORD pid{};
    std::uint64_t created{};
    std::uint64_t area{};
    bool foreground{};
};
struct Search {
    const ApplicationObservation& app;
    std::map<DWORD, std::pair<bool, std::uint64_t>> checked;
    std::vector<Handle> retained;
    std::vector<Candidate> windows;
    unsigned visited = 0;
};
bool same_image(const std::string& first, const std::string& second) {
    const auto left = canonical_file_identity({IdentityKind::Win32Image, first});
    const auto right = canonical_file_identity({IdentityKind::Win32Image, second});
    return CompareStringOrdinal(wide(left.value).c_str(), -1, wide(right.value).c_str(), -1,
                                TRUE) == CSTR_EQUAL;
}
BOOL find_window_impl(HWND window, LPARAM parameter) {
    auto& search = *reinterpret_cast<Search*>(parameter);
    if (++search.visited > 512)
        return FALSE;
    if (!IsWindowVisible(window) || GetAncestor(window, GA_ROOT) != window)
        return TRUE;
    const auto style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if (!(style & WS_EX_APPWINDOW) && ((style & WS_EX_TOOLWINDOW) || GetWindow(window, GW_OWNER)))
        return TRUE;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
        cloaked)
        return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (!pid)
        return TRUE;
    if (!search.checked.contains(pid)) {
        bool matches = false;
        std::uint64_t created = 0;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid));
        if (process && current_user_process(process.get()) &&
            WaitForSingleObject(process.get(), 0) == WAIT_TIMEOUT) {
            FILETIME birth{}, exit{}, kernel{}, user{};
            if (GetProcessTimes(process.get(), &birth, &exit, &kernel, &user)) {
                created = file_time(birth);
                std::wstring image(32768, L'\0');
                DWORD count = static_cast<DWORD>(image.size());
                if (QueryFullProcessImageNameW(process.get(), 0, image.data(), &count)) {
                    image.resize(count);
                    for (const auto& token : search.app.verified_processes)
                        if (token.pid == pid && token.creation_time == created &&
                            same_image(utf8(image), token.executable_path))
                            matches = true;
                    // Audio can belong to a renderer while the taskbar window
                    // belongs to a second instance of the same verified image.
                    // No filename, title or parent-PID guesses are permitted.
                    if (search.app.identity.kind == IdentityKind::Win32Image &&
                        same_image(utf8(image), search.app.identity.value)) {
                        const auto known_pid =
                            std::find_if(search.app.verified_processes.begin(),
                                         search.app.verified_processes.end(),
                                         [pid](const auto& token) { return token.pid == pid; });
                        if (known_pid == search.app.verified_processes.end() ||
                            known_pid->creation_time == created)
                            matches = true;
                    }
                }
            }
            if (matches)
                search.retained.push_back(std::move(process));
        }
        search.checked[pid] = {matches, created};
    }
    const auto [matches, created] = search.checked.at(pid);
    if (!matches || search.windows.size() >= 16)
        return TRUE;
    RECT bounds{};
    GetWindowRect(window, &bounds);
    const auto area = static_cast<std::uint64_t>(std::max(0L, bounds.right - bounds.left)) *
                      static_cast<std::uint64_t>(std::max(0L, bounds.bottom - bounds.top));
    search.windows.push_back({window, pid, created, area, window == GetForegroundWindow()});
    return TRUE;
}
BOOL CALLBACK find_window(HWND window, LPARAM parameter) {
    // No C++ exception may unwind through EnumWindows' native callback boundary.
    try {
        return find_window_impl(window, parameter);
    } catch (...) {
        return TRUE;
    }
}
std::string property(IPropertyStore* store, REFPROPERTYKEY key) {
    // Even malformed metadata must release any COM-owned property allocation.
    struct Value {
        PROPVARIANT data{};
        ~Value() {
            PropVariantClear(&data);
        }
    } value;
    const auto result = store->GetValue(key, &value.data);
    return SUCCEEDED(result) && value.data.vt == VT_LPWSTR && value.data.pwszVal
               ? utf8(value.data.pwszVal)
               : std::string{};
}
} // namespace

WindowIconSnapshot observe_window_icon(const ApplicationObservation& app, unsigned pixels) {
    WindowIconSnapshot result;
    result.signature = "no_taskbar_window";
    if (app.identity.kind != IdentityKind::Win32Image &&
        app.identity.kind != IdentityKind::PackagedApplication)
        return result;
    Search search{app, {}, {}, {}, 0};
    EnumWindows(find_window, reinterpret_cast<LPARAM>(&search));
    std::sort(
        search.windows.begin(), search.windows.end(), [](const auto& first, const auto& second) {
            return first.foreground != second.foreground ? first.foreground > second.foreground
                                                         : first.area > second.area;
        });
    for (const auto& candidate : search.windows) {
        result.icon.reset();
        result.icon_kind.clear();
        result.resource.clear();
        result.app_user_model_id.clear();
        result.class_fallback = false;
        ComPointer<IPropertyStore> properties;
        // Wine exports this entry point as an aborting stub. Its supplemental
        // tests still exercise WM_GETICON; window Shell metadata needs Windows.
        if (!wine_runtime() && SUCCEEDED(SHGetPropertyStoreForWindow(
                                   candidate.window, IID_IPropertyStore, properties.put_void()))) {
            result.resource = property(properties.get(), PKEY_AppUserModel_RelaunchIconResource);
            result.app_user_model_id = property(properties.get(), PKEY_AppUserModel_ID);
        }
        HICON icon = nullptr;
        // Foreign window procedures can be hung. Bound each query and copy the
        // borrowed icon immediately; class icons are only a final window fallback.
        // The large icon can remain the engine/Task Manager icon while the
        // app sets its taskbar icon through WM_SETICON(ICON_SMALL). Prefer the
        // small-window contract before scaling those pixels to our row size.
        for (const auto kind : {ICON_SMALL2, ICON_SMALL, ICON_BIG}) {
            DWORD_PTR value = 0;
            if (SendMessageTimeoutW(candidate.window, WM_GETICON, kind,
                                    MulDiv(static_cast<int>(pixels), 96, 32),
                                    SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT, 50, &value) &&
                value) {
                icon = reinterpret_cast<HICON>(value);
                result.icon_kind = kind == ICON_SMALL2  ? "ICON_SMALL2"
                                   : kind == ICON_SMALL ? "ICON_SMALL"
                                                        : "ICON_BIG";
                break;
            }
        }
        if (!icon) {
            icon = reinterpret_cast<HICON>(GetClassLongPtrW(candidate.window, GCLP_HICONSM));
            result.icon_kind = icon ? "CLASS_SMALL" : "CLASS_BIG";
            if (!icon)
                icon = reinterpret_cast<HICON>(GetClassLongPtrW(candidate.window, GCLP_HICON));
            result.class_fallback = icon != nullptr;
        }
        DWORD current_pid = 0;
        GetWindowThreadProcessId(candidate.window, &current_pid);
        if (current_pid != candidate.pid)
            continue;
        if (const auto copied = icon ? CopyIcon(icon) : nullptr)
            result.icon = std::shared_ptr<void>(
                copied, [](void* value) { DestroyIcon(static_cast<HICON>(value)); });
        result.signature = Document{{"window", reinterpret_cast<std::uintptr_t>(candidate.window)},
                                    {"pid", candidate.pid},
                                    {"created", candidate.created},
                                    {"icon", reinterpret_cast<std::uintptr_t>(icon)},
                                    {"icon_kind", result.icon_kind},
                                    {"resource", result.resource},
                                    {"app_id", result.app_user_model_id}}
                               .dump();
        if (result.icon || !result.resource.empty() || !result.app_user_model_id.empty())
            return result;
    }
    return result;
}
} // namespace audiohole::plug_board::windows_detail
