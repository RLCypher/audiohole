#include "audiohole/plug_board/application_list.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace audiohole::plug_board {
namespace {
std::string trim(std::string name) {
    const auto first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    return name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);
}
} // namespace
std::string normalized_app_name(std::string name) {
    name = application_display_name(trim(std::move(name)));
    for (auto& c : name)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + ('a' - 'A'));
    return name;
}
const std::vector<std::string>& common_host_names() {
    static const std::vector<std::string> names{"powershell",
                                                "pwsh",
                                                "wscript",
                                                "cscript",
                                                "python",
                                                "pythonw",
                                                "java",
                                                "javaw",
                                                "rundll32",
                                                "dllhost",
                                                "svchost",
                                                "audiodg",
                                                "ApplicationFrameHost",
                                                "backgroundTaskHost",
                                                "mshta"};
    return names;
}
std::vector<std::string> normalize_exemptions(const std::vector<std::string>& names) {
    if (names.size() > 512)
        throw std::runtime_error("The app exemption input is too long.");
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (auto name : names) {
        name = trim(std::move(name));
        if (!name.empty() && seen.insert(normalized_app_name(name)).second)
            result.push_back(std::move(name));
        if (result.size() > 256)
            throw std::runtime_error("Use at most 256 exempt app names.");
    }
    return result;
}
std::vector<std::string> parse_exemptions(const std::string& text) {
    if (text.size() > 65536)
        throw std::runtime_error("The app exemption list is too long.");
    std::vector<std::string> names;
    std::size_t start = 0;
    while (start < text.size()) {
        const auto end = text.find_first_of(",\r\n", start);
        auto name = trim(text.substr(start, end == std::string::npos ? end : end - start));
        if (!name.empty())
            names.push_back(std::move(name));
        if (names.size() > 512)
            throw std::runtime_error("Use at most 256 exempt app names.");
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return normalize_exemptions(names);
}
std::string application_host_name(const ApplicationObservation& app) {
    std::string path;
    if (app.identity.kind == IdentityKind::Win32Image)
        path = app.identity.value;
    else if (app.identity.kind == IdentityKind::Unresolved)
        for (const auto& session : app.sessions)
            if (session.process && !session.process->executable_path.empty()) {
                path = session.process->executable_path;
                break;
            }
    const auto slash = path.find_last_of("/\\");
    if (slash != std::string::npos)
        path.erase(0, slash + 1);
    return application_display_name(path);
}
std::string application_group_name(const ApplicationRecord& record) {
    const auto host = normalized_app_name(record.host_name);
    for (const auto& common : common_host_names())
        if (!host.empty() && host == normalized_app_name(common))
            return common;
    return application_display_name(record.detected_name.empty() ? record.display_name
                                                                 : record.detected_name);
}
bool inactive_entry_hidden(const Library& library, const ApplicationRecord& record) {
    if (record.inactive_hidden)
        return true;
    const auto name = normalized_app_name(record.display_name);
    return std::any_of(library.inactive_exempt_apps.begin(), library.inactive_exempt_apps.end(),
                       [&](const auto& exempt) { return normalized_app_name(exempt) == name; });
}
} // namespace audiohole::plug_board
