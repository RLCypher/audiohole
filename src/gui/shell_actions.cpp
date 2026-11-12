#include "gui/shell_actions.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <memory>
// PROPERTYKEY declarations must precede propkey.h in both Windows SDKs.
// clang-format off
#include <initguid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <propvarutil.h>
#include <propkey.h>
// clang-format on
#include <string>
#include <wx/stdpaths.h>

namespace audiohole::gui {
namespace {
struct ShellTask {
    const wchar_t* title;
    const wchar_t* argument;
    const wchar_t* description;
    ShellAction action;
};
constexpr std::array tasks{
    ShellTask{L"Open AudioHole", L"open", L"Show the running AudioHole window.", ShellAction::Open},
    ShellTask{L"AudioHole controls…", L"controls",
              L"Open the full tray menu, including profiles and direction states.",
              ShellAction::Controls},
    ShellTask{L"Start Routing", L"start", L"Start the selected profile in the running session.",
              ShellAction::Start},
    ShellTask{L"Stop Routing", L"stop", L"Stop routing in the running session.", ShellAction::Stop},
    ShellTask{L"Close window", L"close", L"Hide the window to the tray; keep AudioHole running.",
              ShellAction::Close},
    ShellTask{L"Exit AudioHole", L"exit", L"Stop and exit AudioHole completely.",
              ShellAction::Exit}};
struct ReleaseCom {
    void operator()(IUnknown* object) const {
        if (object)
            object->Release();
    }
};
template <class T> using ComOwner = std::unique_ptr<T, ReleaseCom>;
// An uncommitted shell transaction must be aborted, including early HRESULT exits.
struct JumpListTransaction {
    ICustomDestinationList* list;
    bool committed = false;
    ~JumpListTransaction() {
        if (!committed)
            list->AbortList();
    }
};
} // namespace
std::optional<ShellAction> parse_shell_action(std::wstring_view argument) {
    for (const auto& task : tasks)
        if (argument == task.argument)
            return task.action;
    return {};
}
std::optional<ShellAction> decode_shell_action(const COPYDATASTRUCT* packet) {
    if (!packet || packet->dwData != shell_message_tag || !packet->lpData ||
        packet->cbData != sizeof(std::uint32_t))
        return {};
    std::uint32_t value = 0;
    std::memcpy(&value, packet->lpData, sizeof(value));
    for (const auto& task : tasks)
        if (value == static_cast<std::uint32_t>(task.action))
            return task.action;
    return {};
}
HWND find_application_window() {
    HWND found = nullptr;
    EnumWindows(
        [](HWND window, LPARAM result) {
            if (!GetPropW(window, L"AudioHole.GuiWindow"))
                return TRUE;
            *reinterpret_cast<HWND*>(result) = window;
            return FALSE;
        },
        reinterpret_cast<LPARAM>(&found));
    return found;
}
bool forward_shell_action(HWND window, ShellAction action) {
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (!process)
        return false;
    std::wstring image(32768, L'\0');
    DWORD length = static_cast<DWORD>(image.size());
    const bool queried = QueryFullProcessImageNameW(process, 0, image.data(), &length) != FALSE;
    CloseHandle(process);
    if (!queried)
        return false;
    image.resize(length);
    std::error_code error;
    const auto executable =
        std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring());
    if (!std::filesystem::equivalent(std::filesystem::path(image), executable, error) || error)
        return false;
    (void)AllowSetForegroundWindow(process_id);
    auto value = static_cast<std::uint32_t>(action);
    COPYDATASTRUCT packet{shell_message_tag, sizeof(value), &value};
    DWORD_PTR admitted = 0;
    return SendMessageTimeoutW(window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&packet),
                               SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &admitted) &&
           admitted;
}
HRESULT publish_shell_tasks(bool routing_available) {
    ICustomDestinationList* raw_list = nullptr;
    HRESULT result =
        CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER,
                         IID_ICustomDestinationList, reinterpret_cast<void**>(&raw_list));
    if (FAILED(result))
        return result;
    ComOwner<ICustomDestinationList> list(raw_list);
    result = list->SetAppID(shell_app_id);
    if (FAILED(result))
        return result;
    UINT slots = 0;
    IObjectArray* raw_removed = nullptr;
    result = list->BeginList(&slots, IID_IObjectArray, reinterpret_cast<void**>(&raw_removed));
    if (FAILED(result))
        return result;
    ComOwner<IObjectArray> removed(raw_removed);
    JumpListTransaction transaction{list.get()};
    IObjectCollection* raw_collection = nullptr;
    result = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IObjectCollection, reinterpret_cast<void**>(&raw_collection));
    if (FAILED(result))
        return result;
    ComOwner<IObjectCollection> collection(raw_collection);
    const auto executable = wxStandardPaths::Get().GetExecutablePath().ToStdWstring();
    for (const auto& task : tasks) {
        // Windows Jump Lists do not expose disabled tasks. The full shared menu
        // still shows unavailable routing actions disabled in Test Mode.
        if (!routing_available &&
            (task.action == ShellAction::Start || task.action == ShellAction::Stop))
            continue;
        IShellLinkW* raw_link = nullptr;
        result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                  reinterpret_cast<void**>(&raw_link));
        if (FAILED(result))
            return result;
        ComOwner<IShellLinkW> link(raw_link);
        const auto arguments = std::wstring(L"--taskbar-action ") + task.argument;
        if (FAILED(result = link->SetPath(executable.c_str())) ||
            FAILED(result = link->SetArguments(arguments.c_str())) ||
            FAILED(result = link->SetDescription(task.description)) ||
            FAILED(result = link->SetIconLocation(executable.c_str(), 0)))
            return result;
        IPropertyStore* raw_properties = nullptr;
        result =
            link->QueryInterface(IID_IPropertyStore, reinterpret_cast<void**>(&raw_properties));
        if (FAILED(result))
            return result;
        ComOwner<IPropertyStore> properties(raw_properties);
        PROPVARIANT title;
        PropVariantInit(&title);
        result = InitPropVariantFromString(task.title, &title);
        if (SUCCEEDED(result))
            result = properties->SetValue(PKEY_Title, title);
        PropVariantClear(&title);
        if (FAILED(result) || FAILED(result = properties->Commit()) ||
            FAILED(result = collection->AddObject(link.get())))
            return result;
    }
    result = list->AddUserTasks(collection.get());
    if (FAILED(result))
        return result;
    result = list->CommitList();
    transaction.committed = SUCCEEDED(result);
    return result;
}
Result configure_windows_startup(bool enabled) {
    // Explicit user opt-in, current user only. No elevation or scheduled task.
    std::wstring executable(32768, L'\0');
    const auto count =
        GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!count || count == executable.size())
        return {false, "Cannot resolve the application path for Windows startup."};
    executable.resize(count);
    const auto command = L"\"" + executable + L"\"";
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                        nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return {false, "Windows startup preference could not be changed."};
    const auto result =
        enabled ? RegSetValueExW(key, L"AudioHole", 0, REG_SZ,
                                 reinterpret_cast<const BYTE*>(command.c_str()),
                                 static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)))
                : RegDeleteValueW(key, L"AudioHole");
    RegCloseKey(key);
    return {result == ERROR_SUCCESS || (!enabled && result == ERROR_FILE_NOT_FOUND),
            "Windows startup preference updated."};
}
std::filesystem::path first_run_marker_path() {
    return std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring())
               .parent_path() /
           "AudioHole.has-run";
}
Result show_in_explorer(const std::filesystem::path& file) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error) || error)
        return {false, "The saved JSON file is unavailable. Save or reload the record first."};
    const auto item = ILCreateFromPathW(file.c_str());
    if (!item)
        return {false, "Windows could not resolve the saved JSON file."};
    const auto result = SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
    CoTaskMemFree(item);
    return {SUCCEEDED(result), SUCCEEDED(result)
                                   ? "Saved JSON selected in Explorer."
                                   : "Windows could not open Explorer for this file."};
}
} // namespace audiohole::gui
