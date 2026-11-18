#include "native_runtime.hpp"

#include <tlhelp32.h>

#include <set>

namespace audiohole::plug_board::windows_detail {
namespace {
std::string key(const ApplicationIdentity& identity) {
    return std::to_string(static_cast<int>(identity.kind)) + ':' + identity.value;
}
std::vector<unsigned char> user_sid(HANDLE process) {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &raw))
        return {};
    Handle token(raw);
    DWORD bytes = 0;
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &bytes);
    if (bytes == 0 || bytes > 65536)
        return {};
    std::vector<unsigned char> buffer(bytes);
    if (!GetTokenInformation(token.get(), TokenUser, buffer.data(), bytes, &bytes))
        return {};
    const auto* information = reinterpret_cast<const TOKEN_USER*>(buffer.data());
    if (!IsValidSid(information->User.Sid))
        return {};
    std::vector<unsigned char> sid(GetLengthSid(information->User.Sid));
    if (!CopySid(static_cast<DWORD>(sid.size()), sid.data(), information->User.Sid))
        return {};
    return sid;
}
} // namespace

bool current_user_process(HANDLE process) {
    DWORD current_session = 0, target_session = 0;
    const auto pid = GetProcessId(process);
    if (!pid || !ProcessIdToSessionId(GetCurrentProcessId(), &current_session) ||
        !ProcessIdToSessionId(pid, &target_session) || current_session != target_session)
        return false;
    const auto current = user_sid(GetCurrentProcess());
    const auto target = user_sid(process);
    return !current.empty() && !target.empty() &&
           EqualSid(const_cast<unsigned char*>(current.data()),
                    const_cast<unsigned char*>(target.data()));
}

ApplicationIdentity canonical_file_identity(const ApplicationIdentity& identity) {
    if (identity.kind != IdentityKind::Win32Image)
        return identity;
    const auto path = wide(identity.value);
    Handle file(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file)
        return identity;
    std::wstring final(32768, L'\0');
    const auto count = GetFinalPathNameByHandleW(
        file.get(), final.data(), static_cast<DWORD>(final.size()), FILE_NAME_NORMALIZED);
    if (!count || count >= final.size())
        return identity;
    final.resize(count);
    return {identity.kind, utf8(final)};
}

std::vector<ProcessEvidence>
find_current_user_processes(const std::vector<ApplicationIdentity>& identities) {
    std::set<std::string> wanted;
    for (const auto& identity : identities)
        wanted.insert(key(canonical_file_identity(identity)));
    std::vector<ProcessEvidence> matches;
    if (wanted.empty() || wine_runtime())
        return matches;
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot)
        throw std::runtime_error("Cannot take the initial pending-process snapshot.");
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.get(), &entry))
        return matches;
    do {
        if (entry.th32ProcessID <= 4)
            continue;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                                   entry.th32ProcessID));
        if (!process || !current_user_process(process.get()))
            continue;
        const auto metadata = inspect_process(entry.th32ProcessID);
        if (metadata.result.ok && metadata.identity.kind != IdentityKind::Unresolved &&
            wanted.contains(key(metadata.identity)))
            matches.push_back({metadata.identity, metadata.token});
    } while (Process32NextW(snapshot.get(), &entry));
    return matches;
}

} // namespace audiohole::plug_board::windows_detail
