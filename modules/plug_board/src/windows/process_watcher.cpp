#include "audiohole/plug_board/windows.hpp"
#include "support.hpp"

#include <wbemidl.h>

namespace audiohole::plug_board {
using namespace windows_detail;
namespace {

class AutomationString {
  public:
    explicit AutomationString(const std::wstring& text) : value_(SysAllocString(text.c_str())) {
        if (!value_) {
            throw std::runtime_error("Could not allocate WMI query string.");
        }
    }
    ~AutomationString() {
        SysFreeString(value_);
    }
    AutomationString(const AutomationString&) = delete;
    AutomationString& operator=(const AutomationString&) = delete;
    BSTR get() const {
        return value_;
    }

  private:
    BSTR value_ = nullptr;
};

} // namespace

ProcessStartProbe
probe_process_start_notifications(const std::function<ProcessToken()>& start_target,
                                  std::chrono::milliseconds timeout) {
    ProcessStartProbe probe;
    try {
        if (timeout.count() < 1 || timeout > std::chrono::seconds(30)) {
            throw std::runtime_error("WMI probe timeout must be between 1 and 30000 milliseconds.");
        }
        Apartment apartment;
        require(apartment.result(), "Initialize process-watcher MTA");
        ComPointer<IWbemLocator> locator;
        require(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                                 locator.put_void()),
                "Create WMI locator");
        AutomationString scope(L"ROOT\\CIMV2");
        ComPointer<IWbemServices> services;
        require(locator->ConnectServer(scope.get(), nullptr, nullptr, nullptr, 0, nullptr, nullptr,
                                       services.put()),
                "Connect local WMI using current user");
        require(CoSetProxyBlanket(services.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                                  RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr,
                                  EOAC_NONE),
                "Set local WMI proxy authentication");
        DWORD session_id = 0;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id)) {
            throw std::runtime_error("Could not determine the current Windows logon session.");
        }
        AutomationString language(L"WQL");
        AutomationString query(L"SELECT * FROM Win32_ProcessStartTrace WHERE SessionID = " +
                               std::to_wstring(session_id));
        ComPointer<IEnumWbemClassObject> events;
        const auto subscribed = services->ExecNotificationQuery(
            language.get(), query.get(), WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY,
            nullptr, events.put());
        probe.subscription =
            native_result(subscribed, "Subscribe to process-start events without elevation");
        if (FAILED(subscribed)) {
            return probe;
        }
        // Register before creating the disposable process. No periodic process
        // enumeration is used to compensate for registration races or failure.
        const auto target = start_target();
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline && probe.unrelated_events < 256) {
            ComPointer<IWbemClassObject> event;
            ULONG returned = 0;
            const auto next = events->Next(100, 1, event.put(), &returned);
            require(next, "Wait for process-start event");
            if (returned == 0) {
                continue;
            }
            VARIANT pid;
            VariantInit(&pid);
            const auto read = event->Get(L"ProcessID", 0, &pid, nullptr, nullptr);
            const bool matches =
                SUCCEEDED(read) &&
                ((pid.vt == VT_UI4 && pid.ulVal == target.pid) ||
                 (pid.vt == VT_I4 && static_cast<std::uint32_t>(pid.lVal) == target.pid));
            VariantClear(&pid);
            if (!matches) {
                ++probe.unrelated_events;
                continue;
            }
            probe.target_observed = true;
            const auto metadata = inspect_process(target.pid);
            probe.identity_verified = metadata.result.ok && metadata.token == target;
            probe.detail = probe.identity_verified
                               ? "Target start event and exact live identity verified."
                               : "Event arrived, but the live identity was not verified.";
            return probe;
        }
        probe.detail = "No verified target event before the bounded wait ended; background startup "
                       "remains unproven.";
    } catch (const std::exception& error) {
        if (!probe.subscription.ok) {
            probe.subscription = {false, "process_watcher_unavailable", error.what()};
        }
        probe.detail = error.what();
    }
    return probe;
}

} // namespace audiohole::plug_board
