#include "audiohole/plug_board/windows.hpp"
#include "support.hpp"

#include <appmodel.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <winternl.h>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <map>

namespace audiohole::plug_board {
using namespace windows_detail;
namespace {

// Microsoft documents this success status for GetProcessId, but some Windows
// SDK/MinGW headers omit its named constant. It means a creator PID, not sole ownership.
constexpr HRESULT session_spans_processes = static_cast<HRESULT>(0x0889000D);

// Keep display casing and ask the filesystem for the final path. Blindly
// lowercasing would merge distinct files inside case-sensitive directories.
std::wstring final_image_path(const std::wstring& image) {
    Handle file(CreateFileW(image.c_str(), FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        return image;
    }
    std::wstring path(32768, L'\0');
    const auto length = GetFinalPathNameByHandleW(
        file.get(), path.data(), static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED);
    if (!length || length >= path.size()) {
        return image;
    }
    path.resize(length);
    return path;
}

// Query metadata without requesting debug privileges, process memory or access
// to another user's token. The process handle stays owned through all queries.
ProcessMetadata process_metadata(std::uint32_t pid) {
    ProcessMetadata metadata;
    if (wine_runtime()) {
        metadata.result = {false, "unsupported_environment",
                           "Native package/process identity is unavailable in Wine; its API may be "
                           "an aborting stub."};
        return metadata;
    }
    if (pid == 0) {
        metadata.result = {false, "no_process", "PID zero is not a process identity."};
        return metadata;
    }
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid));
    if (!process) {
        metadata.result = native_result(HRESULT_FROM_WIN32(GetLastError()), "OpenProcess metadata");
        return metadata;
    }
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process.get(), &created, &exited, &kernel, &user)) {
        metadata.result = native_result(HRESULT_FROM_WIN32(GetLastError()), "GetProcessTimes");
        return metadata;
    }
    std::wstring image(32768, L'\0');
    DWORD count = static_cast<DWORD>(image.size());
    if (!QueryFullProcessImageNameW(process.get(), 0, image.data(), &count)) {
        metadata.result =
            native_result(HRESULT_FROM_WIN32(GetLastError()), "QueryFullProcessImageNameW");
        return metadata;
    }
    image.resize(count);
    metadata.token = {pid, file_time(created), utf8(final_image_path(image))};
    metadata.identity = {IdentityKind::Win32Image, metadata.token.executable_path};
    metadata.display_name = utf8(std::filesystem::path(image).filename().wstring());

    // Packaged apps can share an executable/broker. A verified application ID
    // wins over that host path; never replace it with the package's first app.
    UINT32 id_length = 0;
    const auto initial = GetApplicationUserModelId(process.get(), &id_length, nullptr);
    if (initial == ERROR_INSUFFICIENT_BUFFER && id_length > 1 && id_length <= 32768) {
        std::wstring id(id_length, L'\0');
        if (GetApplicationUserModelId(process.get(), &id_length, id.data()) == ERROR_SUCCESS) {
            id.resize(id_length > 0 ? id_length - 1 : 0);
            if (!id.empty()) {
                metadata.identity = {IdentityKind::PackagedApplication, utf8(id)};
                metadata.token.app_user_model_id = metadata.identity.value;
            }
        }
    }
    if (metadata.token.app_user_model_id.empty()) {
        // A known interpreter/broker image is not enough to identify its hosted
        // application. Keep owners provisional instead of merging unrelated
        // scripts/services by their shared executable. More general host metadata
        // resolution belongs to the application-identity milestone.
        const wchar_t* shared_hosts[]{L"python.exe",
                                      L"pythonw.exe",
                                      L"java.exe",
                                      L"javaw.exe",
                                      L"rundll32.exe",
                                      L"dllhost.exe",
                                      L"svchost.exe",
                                      L"audiodg.exe",
                                      L"ApplicationFrameHost.exe",
                                      L"backgroundTaskHost.exe",
                                      L"powershell.exe",
                                      L"pwsh.exe",
                                      L"wscript.exe",
                                      L"cscript.exe",
                                      L"mshta.exe"};
        const auto filename = std::filesystem::path(image).filename().wstring();
        const bool shared_host = std::any_of(
            std::begin(shared_hosts), std::end(shared_hosts), [&](const wchar_t* expected) {
                return CompareStringOrdinal(filename.c_str(), static_cast<int>(filename.size()),
                                            expected, -1, TRUE) == CSTR_EQUAL;
            });
        if (shared_host) {
            metadata.identity = {};
        }
    }
    if (WaitForSingleObject(process.get(), 0) != WAIT_TIMEOUT) {
        metadata.result = {false, "process_exited", "Process exited while collecting metadata."};
        return metadata;
    }
    metadata.result = {true, "verified_process", {}};
    return metadata;
}

void read_sessions(IMMDevice* device, const std::string& endpoint,
                   std::map<std::uint32_t, ProcessMetadata>& processes, SessionSnapshot& snapshot) {
    ComPointer<IAudioSessionManager2> manager;
    require(device->Activate(IID_IAudioSessionManager2, CLSCTX_ALL, nullptr, manager.put_void()),
            "Activate session manager");
    ComPointer<IAudioSessionEnumerator> sessions;
    require(manager->GetSessionEnumerator(sessions.put()), "GetSessionEnumerator");
    int count = 0;
    require(sessions->GetCount(&count), "Get session count");
    for (int index = 0; index < count; ++index) {
        try {
            ComPointer<IAudioSessionControl> session;
            ComPointer<IAudioSessionControl2> extended;
            require(sessions->GetSession(index, session.put()), "GetSession");
            require(session->QueryInterface(IID_IAudioSessionControl2, extended.put_void()),
                    "Query IAudioSessionControl2");
            SessionObservation observation;
            observation.endpoint_id = endpoint;
            TaskString instance, session_id, name, icon;
            require(extended->GetSessionInstanceIdentifier(instance.put()),
                    "Get session instance ID");
            observation.instance_id = utf8(instance.get());
            if (SUCCEEDED(extended->GetSessionIdentifier(session_id.put()))) {
                observation.session_id = utf8(session_id.get());
            }
            if (SUCCEEDED(session->GetDisplayName(name.put()))) {
                observation.display_name = utf8(name.get());
            }
            if (SUCCEEDED(session->GetIconPath(icon.put()))) {
                observation.icon_path = utf8(icon.get());
            }
            AudioSessionState state = AudioSessionStateInactive;
            const auto state_result = session->GetState(&state);
            observation.state = FAILED(state_result)                 ? SessionState::Unknown
                                : state == AudioSessionStateActive   ? SessionState::Active
                                : state == AudioSessionStateInactive ? SessionState::Inactive
                                : state == AudioSessionStateExpired  ? SessionState::Expired
                                                                     : SessionState::Unknown;
            if (observation.state == SessionState::Unknown) {
                snapshot.complete = false;
                snapshot.diagnostics.push_back(
                    native_result(state_result, "Get session activity").code + " on " + endpoint);
            }
            const auto system_result = extended->IsSystemSoundsSession();
            if (system_result == S_OK) {
                observation.identity = {IdentityKind::SystemSounds, "windows-system-sounds"};
                observation.display_name = "System Sounds";
            } else {
                if (FAILED(system_result)) {
                    snapshot.complete = false;
                }
                DWORD pid = 0;
                const auto pid_result = extended->GetProcessId(&pid);
                observation.cross_process = pid_result == session_spans_processes;
                if (SUCCEEDED(pid_result) && pid != 0) {
                    auto [entry, inserted] = processes.try_emplace(pid);
                    if (inserted) {
                        entry->second = process_metadata(pid);
                    }
                    if (entry->second.result.ok) {
                        observation.process = entry->second.token;
                        observation.identity = entry->second.identity;
                        if (observation.display_name.empty()) {
                            observation.display_name = entry->second.display_name;
                        }
                    } else {
                        snapshot.complete = false;
                        snapshot.diagnostics.push_back("PID " + std::to_string(pid) + ": " +
                                                       entry->second.result.code + " " +
                                                       entry->second.result.detail);
                    }
                } else {
                    snapshot.complete = false;
                    snapshot.diagnostics.push_back(
                        native_result(pid_result, "Get session process ID").code + " on " +
                        endpoint);
                }
            }
            if (observation.display_name.empty()) {
                observation.display_name = "Unidentified application";
            }
            snapshot.sessions.push_back(std::move(observation));
        } catch (const std::exception& error) {
            // Session creation/destruction races are normal observations, but
            // an incomplete snapshot must never become confirmed absence.
            snapshot.complete = false;
            snapshot.diagnostics.push_back(endpoint + ": " + error.what());
        }
    }
}

} // namespace

WindowsEnvironment inspect_windows_environment() {
    WindowsEnvironment environment;
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    environment.wine = wine_runtime();
    using GetVersion = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto get_version =
        ntdll ? std::bit_cast<GetVersion>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
    OSVERSIONINFOEXW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (get_version && get_version(reinterpret_cast<PRTL_OSVERSIONINFOW>(&version)) == 0) {
        environment.major = version.dwMajorVersion;
        environment.minor = version.dwMinorVersion;
        environment.build = version.dwBuildNumber;
        environment.workstation = version.wProductType == VER_NT_WORKSTATION;
    } else {
        environment.detail = "Exact Windows version could not be determined.";
    }
    DWORD update_revision = 0;
    DWORD update_size = sizeof(update_revision);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"UBR",
                     RRF_RT_REG_DWORD, nullptr, &update_revision, &update_size) == ERROR_SUCCESS) {
        environment.update_build_revision = update_revision;
    }
    HANDLE raw_token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw_token)) {
        Handle token(raw_token);
        TOKEN_ELEVATION elevation{};
        DWORD size = 0;
        if (GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation),
                                &size)) {
            environment.elevated = elevation.TokenIsElevated != 0;
            environment.privilege_known = true;
        }
    }
    return environment;
}

ProcessMetadata inspect_process(std::uint32_t pid) {
    return process_metadata(pid);
}

WindowsInspection inspect_windows_sessions(bool include_sessions) {
    WindowsInspection inspection;
    inspection.environment = inspect_windows_environment();
    try {
        Apartment apartment;
        require(apartment.result(), "Initialize inspection MTA");
        ComPointer<IMMDeviceEnumerator> enumerator;
        require(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                 IID_IMMDeviceEnumerator, enumerator.put_void()),
                "Create MMDevice enumerator");
        for (std::size_t index = 0; index < render_roles.size(); ++index) {
            ComPointer<IMMDevice> device;
            if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, static_cast<ERole>(index),
                                                              device.put()))) {
                TaskString id;
                if (SUCCEEDED(device->GetId(id.put()))) {
                    inspection.default_endpoints[index] = utf8(id.get());
                }
            }
        }
        ComPointer<IMMDeviceCollection> devices;
        require(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATEMASK_ALL, devices.put()),
                "Enumerate render endpoints");
        UINT count = 0;
        require(devices->GetCount(&count), "Get render endpoint count");
        std::map<std::uint32_t, ProcessMetadata> processes;
        for (UINT index = 0; index < count; ++index) {
            try {
                ComPointer<IMMDevice> device;
                require(devices->Item(index, device.put()), "Get render endpoint");
                TaskString id;
                require(device->GetId(id.put()), "Get endpoint ID");
                DWORD state = 0;
                require(device->GetState(&state), "Get endpoint state");
                RenderEndpoint endpoint{utf8(id.get()), {}, state == DEVICE_STATE_ACTIVE, state};
                ComPointer<IPropertyStore> properties;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, properties.put()))) {
                    PROPVARIANT name;
                    PropVariantInit(&name);
                    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &name)) &&
                        name.vt == VT_LPWSTR && name.pwszVal) {
                        endpoint.display_name = utf8(name.pwszVal);
                    }
                    PropVariantClear(&name);
                }
                inspection.endpoints.push_back(endpoint);
                if (endpoint.available && include_sessions) {
                    read_sessions(device.get(), endpoint.endpoint_id, processes,
                                  inspection.snapshot);
                }
            } catch (const std::exception& error) {
                inspection.snapshot.complete = false;
                inspection.snapshot.diagnostics.push_back(error.what());
            }
        }
    } catch (const std::exception& error) {
        inspection.snapshot.complete = false;
        inspection.snapshot.diagnostics.push_back(error.what());
    }
    return inspection;
}

} // namespace audiohole::plug_board
