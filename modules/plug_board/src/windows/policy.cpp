#include "audiohole/plug_board/windows.hpp"
#include "native_runtime.hpp"
#include "policy_abi.hpp"
#include "support.hpp"

#include <algorithm>
#include <map>
#include <thread>

namespace audiohole::plug_board {
using namespace windows_detail;
namespace {

constexpr wchar_t policy_prefix[] = L"\\\\?\\SWD#MMDEVAPI#";
constexpr wchar_t render_suffix[] = L"#{e6327cad-dcec-4949-ae8a-991e976a79d2}";

class WindowsPolicy final : public PolicyAdapter {
  public:
    explicit WindowsPolicy(std::vector<ProcessToken> allowlist, PolicyScope scope,
                           std::function<void(bool)> observer)
        : owner_(std::this_thread::get_id()), allowlist_(std::move(allowlist)), scope_(scope),
          observer_(std::move(observer)) {
        const auto environment = inspect_windows_environment();
        if (environment.wine || !environment.workstation || environment.major != 10 ||
            environment.build < 22000) {
            capability_ = {
                false, "unsupported_environment",
                "Private-policy writes require native Windows 11; Wine is not validation."};
            return;
        }
        capability_ = native_result(apartment_.result(), "Initialize policy WinRT MTA");
        if (!capability_.ok) {
            return;
        }
        RuntimeString class_name(L"Windows.Media.Internal.AudioPolicyConfig");
        capability_ = native_result(
            RoGetActivationFactory(class_name.get(), policy_factory_21h2_iid, factory_.put_void()),
            "Activate known private policy IID; native compatibility is still unvalidated");
        if (capability_.ok) {
            for (const auto& process : allowlist_) {
                Handle handle(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                                          process.pid));
                if (handle) {
                    process_handles_.try_emplace(process.pid, std::move(handle));
                } else {
                    const auto error = GetLastError();
                    process_open_errors_[process.pid] =
                        error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_FOUND
                            ? OperationResult{false, "process_not_live",
                                              "The verified application process is no longer "
                                              "available."}
                            : native_result(HRESULT_FROM_WIN32(error),
                                            "Open verified policy process");
                }
            }
        }
    }

    bool simulated() const override {
        return false;
    }
    OperationResult capability() override {
        if (std::this_thread::get_id() != owner_) {
            return {false, "wrong_thread",
                    "Policy COM objects belong to their constructing thread."};
        }
        return capability_;
    }
    OperationResult validate_process(const ProcessToken& expected) override {
        if (scope_ == PolicyScope::UserAssignment &&
            (expected.pid == GetCurrentProcessId() ||
             is_audiohole_application({IdentityKind::Win32Image, expected.executable_path})))
            return {false, "internal_application",
                    "AudioHole processes are excluded from application routing."};
        if (!capability().ok) {
            return capability();
        }
        if (expected.pid <= 4 ||
            (scope_ == PolicyScope::FixtureExperiment && expected.pid == GetCurrentProcessId()) ||
            std::find(allowlist_.begin(), allowlist_.end(), expected) == allowlist_.end()) {
            return {false, "not_allowlisted",
                    "Policy target is not an explicitly verified allowed application process."};
        }
        const auto retained = process_handles_.find(expected.pid);
        if (retained == process_handles_.end() && process_open_errors_.contains(expected.pid))
            return process_open_errors_.at(expected.pid);
        if (retained == process_handles_.end() ||
            WaitForSingleObject(retained->second.get(), 0) != WAIT_TIMEOUT) {
            return {false, "process_not_live",
                    "The retained allowlisted process handle is no longer live."};
        }
        if (!current_user_process(retained->second.get())) {
            return {false, "different_user_or_session",
                    "Routing is restricted to the current user and Windows logon session."};
        }
        const auto current = inspect_process(expected.pid);
        if (!current.result.ok) {
            return current.result;
        }
        if (current.identity.kind == IdentityKind::Unresolved) {
            return {false, "application_identity_uncertain",
                    "A shared host without verified application identity is not a routing target."};
        }
        if (current.token != expected) {
            return {false, "process_identity_changed",
                    "PID, creation time or executable changed; no call permitted."};
        }
        return {true, "verified_process", {}};
    }

    PreferenceRead read(const ProcessToken& process, AudioRole role) override {
        const auto validated = validate_process(process);
        if (!validated.ok) {
            return {validated, {}};
        }
        RuntimeString value;
        PreferenceRead preference;
        if (observer_)
            observer_(false);
        preference.result = native_result(factory_->GetPersistedDefaultAudioEndpoint(
                                              process.pid, eRender, native_role(role), value.put()),
                                          "Read per-role application preference");
        if (!preference.result.ok) {
            return preference;
        }
        UINT32 count = 0;
        const auto text = WindowsGetStringRawBuffer(value.get(), &count);
        if (text && count > 0) {
            std::wstring encoded(text, count);
            const std::wstring prefix(policy_prefix), suffix(render_suffix);
            if (!encoded.starts_with(prefix) || !encoded.ends_with(suffix) ||
                encoded.size() <= prefix.size() + suffix.size()) {
                preference.result = {
                    false, "unknown_policy_encoding",
                    "Returned preference is not a recognized render device string."};
                return preference;
            }
            preference.endpoint_id =
                utf8(encoded.substr(prefix.size(), encoded.size() - prefix.size() - suffix.size()));
        }
        const auto still_live = validate_process(process);
        if (!still_live.ok) {
            preference.result = still_live;
            return preference;
        }
        // Retain the first positively read value. Only this exact original may
        // be restored even if its endpoint becomes unavailable during the test.
        originals_.try_emplace(std::make_pair(process.pid, role), preference.endpoint_id);
        return preference;
    }

    OperationResult write(const ProcessToken& process, AudioRole role,
                          const std::optional<std::string>& endpoint) override {
        const auto validated = validate_process(process);
        if (!validated.ok) {
            return validated;
        }
        if (endpoint && endpoint->empty()) {
            return {false, "empty_endpoint", "Use an explicit ID or null for Windows default."};
        }
        const auto original = originals_.find({process.pid, role});
        if (original == originals_.end()) {
            return {false, "missing_original",
                    "Read the original role before permitting any setter."};
        }
        if (endpoint && endpoint != original->second) {
            const auto endpoint_check = validate_destination(*endpoint);
            if (!endpoint_check.ok) {
                return endpoint_check;
            }
        }
        // A null HSTRING clears this process/role's override, not system defaults
        // and never every application's preferences. Originals use the same path.
        const std::wstring encoded =
            endpoint ? std::wstring(policy_prefix) + wide(*endpoint) + render_suffix
                     : std::wstring{};
        RuntimeString value(encoded);
        if (observer_)
            observer_(true);
        const auto result = native_result(
            factory_->SetPersistedDefaultAudioEndpoint(process.pid, eRender, native_role(role),
                                                       endpoint ? value.get() : nullptr),
            "Write allowlisted process render-role preference");
        const auto still_live = validate_process(process);
        return still_live.ok ? result : still_live;
    }

  private:
    // Declaration order makes factory release precede apartment uninitialization.
    Apartment apartment_;
    ComPointer<PolicyFactory21H2> factory_;
    std::thread::id owner_;
    std::vector<ProcessToken> allowlist_;
    PolicyScope scope_;
    std::function<void(bool)> observer_;
    std::map<std::uint32_t, OperationResult> process_open_errors_;
    OperationResult capability_;
    std::map<std::pair<std::uint32_t, AudioRole>, std::optional<std::string>> originals_;
    // Retain the process objects across PID-based policy calls, not just during
    // individual metadata queries. Never reopen a replacement PID silently.
    std::map<std::uint32_t, Handle> process_handles_;

    static ERole native_role(AudioRole role) {
        // ERole is an enumeration, not a bitmask. Each role gets a separate call.
        switch (role) {
        case AudioRole::Console:
            return eConsole;
        case AudioRole::Multimedia:
            return eMultimedia;
        case AudioRole::Communications:
            return eCommunications;
        }
        throw std::runtime_error("Invalid audio role.");
    }

    static OperationResult validate_destination(const std::string& endpoint) {
        ComPointer<IMMDeviceEnumerator> enumerator;
        auto result = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                       IID_IMMDeviceEnumerator, enumerator.put_void());
        if (FAILED(result)) {
            return native_result(result, "Create destination validator");
        }
        ComPointer<IMMDevice> device;
        result = enumerator->GetDevice(wide(endpoint).c_str(), device.put());
        if (FAILED(result)) {
            return native_result(result, "Resolve explicit destination");
        }
        ComPointer<IMMEndpoint> endpoint_interface;
        result = device->QueryInterface(IID_IMMEndpoint, endpoint_interface.put_void());
        if (FAILED(result)) {
            return native_result(result, "Read destination direction");
        }
        EDataFlow flow = eAll;
        DWORD state = 0;
        result = endpoint_interface->GetDataFlow(&flow);
        if (FAILED(result)) {
            return native_result(result, "GetDataFlow");
        }
        result = device->GetState(&state);
        if (FAILED(result)) {
            return native_result(result, "Get destination availability");
        }
        if (flow != eRender || state != DEVICE_STATE_ACTIVE) {
            return {false, "unavailable_destination",
                    "Temporary target must be an available render endpoint."};
        }
        return {true, "available_render_destination", {}};
    }
};

} // namespace

std::unique_ptr<PolicyAdapter> make_windows_policy_adapter(std::vector<ProcessToken> allowlist,
                                                           PolicyScope scope,
                                                           std::function<void(bool)> observer) {
    return std::make_unique<WindowsPolicy>(std::move(allowlist), scope, std::move(observer));
}

} // namespace audiohole::plug_board
