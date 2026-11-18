#pragma once

#include <string>
#include <string_view>

namespace audiohole::plug_board::windows_detail {

// Shell parsing does not accept every path returned by GetFinalPathNameByHandleW.
// Return a display-only copy with the extended DOS prefix removed when ordinary
// Win32 parsing names the same file. Never change an identity or ProcessToken.
// Keep other namespaces and normalization-sensitive names intact: removing the
// prefix from a trailing-dot/space component or a dot segment can name a different
// file. Such inputs may still fail icon lookup and retain an explicit diagnostic.
// This pure helper performs no file access, environment expansion or case folding.
inline std::wstring shell_image_path(std::wstring_view verified_image) {
    const auto ascii_drive = [](wchar_t character) {
        return (character >= L'A' && character <= L'Z') || (character >= L'a' && character <= L'z');
    };
    if (!verified_image.starts_with(L"\\\\?\\") || verified_image.size() < 7 ||
        !ascii_drive(verified_image[4]) || verified_image[5] != L':' ||
        verified_image[6] != L'\\') {
        return std::wstring(verified_image);
    }
    const auto ordinary = verified_image.substr(4);
    auto remainder = ordinary.substr(3);
    while (!remainder.empty()) {
        const auto end = remainder.find(L'\\');
        const auto component = remainder.substr(0, end);
        if (component.empty() || component.back() == L'.' || component.back() == L' ' ||
            component.find(L':') != std::wstring_view::npos ||
            component.find(L'/') != std::wstring_view::npos) {
            return std::wstring(verified_image);
        }
        if (end == std::wstring_view::npos) {
            break;
        }
        remainder.remove_prefix(end + 1);
    }
    return std::wstring(ordinary);
}

} // namespace audiohole::plug_board::windows_detail
