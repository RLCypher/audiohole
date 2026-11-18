#include "native_runtime.hpp"

#include <wbemidl.h>

#include <deque>
#include <future>
#include <mutex>
#include <set>
#include <thread>

namespace audiohole::plug_board::windows_detail {
namespace {
class BString {
  public:
    explicit BString(const std::wstring& value) : value_(SysAllocString(value.c_str())) {
        if (!value_)
            throw std::bad_alloc();
    }
    ~BString() {
        SysFreeString(value_);
    }
    BSTR get() const {
        return value_;
    }

  private:
    BSTR value_;
};
struct ProcessSignal {
    std::mutex mutex;
    Handle changed{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    std::deque<DWORD> starts;
    std::optional<
        std::pair<std::vector<ApplicationIdentity>, std::shared_ptr<std::promise<OperationResult>>>>
        pending;
    std::optional<HRESULT> failed;
    bool accepting = true;
    std::shared_ptr<NativeCounters> counters;
};
class ProcessSink final : public IWbemObjectSink {
  public:
    explicit ProcessSink(std::shared_ptr<ProcessSignal> signal) : signal_(std::move(signal)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** pointer) override {
        if (!pointer)
            return E_POINTER;
        *pointer = nullptr;
        if (id == IID_IUnknown || id == IID_IWbemObjectSink) {
            *pointer = static_cast<IWbemObjectSink*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto value = --references_;
        if (!value)
            delete this;
        return value;
    }
    HRESULT STDMETHODCALLTYPE Indicate(LONG count, IWbemClassObject** objects) override {
        // Only copy PIDs from the provider's in-memory event objects. Identity,
        // ownership and executable queries belong to the watcher MTA worker.
        for (LONG index = 0; index < std::min<LONG>(count, 256); ++index) {
            VARIANT value;
            VariantInit(&value);
            const auto result = objects[index]->Get(L"ProcessID", 0, &value, nullptr, nullptr);
            const DWORD pid = SUCCEEDED(result) && value.vt == VT_UI4 ? value.ulVal
                              : SUCCEEDED(result) && value.vt == VT_I4 && value.lVal > 0
                                  ? static_cast<DWORD>(value.lVal)
                                  : 0;
            VariantClear(&value);
            if (pid <= 4)
                continue;
            std::lock_guard lock(signal_->mutex);
            if (!signal_->accepting)
                return WBEM_S_NO_ERROR;
            if (signal_->starts.size() < 256)
                signal_->starts.push_back(pid);
            else
                ++signal_->counters->dropped;
            SetEvent(signal_->changed.get());
        }
        if (count > 256)
            signal_->counters->dropped += static_cast<std::uint64_t>(count - 256);
        return WBEM_S_NO_ERROR;
    }
    HRESULT STDMETHODCALLTYPE SetStatus(LONG flags, HRESULT result, BSTR,
                                        IWbemClassObject*) override {
        if (flags == WBEM_STATUS_COMPLETE) {
            std::lock_guard lock(signal_->mutex);
            if (signal_->accepting) {
                signal_->failed = FAILED(result) ? result : E_ABORT;
                SetEvent(signal_->changed.get());
            }
        }
        return WBEM_S_NO_ERROR;
    }

  private:
    std::atomic<ULONG> references_{1};
    std::shared_ptr<ProcessSignal> signal_;
};
std::string identity_key(const ApplicationIdentity& identity) {
    return std::to_string(static_cast<int>(identity.kind)) + ':' + identity.value;
}
} // namespace

struct ProcessWatcher::Implementation {
    BackendCallbacks callbacks;
    std::shared_ptr<NativeCounters> counters;
    std::shared_ptr<ProcessSignal> signal = std::make_shared<ProcessSignal>();
    std::set<std::string> wanted;
    OperationResult cleanup{true, "process_watch_stopped", "Process-start notification released."};
    OperationResult started;
    std::promise<OperationResult> started_promise;
    std::shared_future<OperationResult> started_future = started_promise.get_future().share();
    std::jthread worker;
    Implementation(BackendCallbacks value, std::shared_ptr<NativeCounters> metrics)
        : callbacks(std::move(value)), counters(std::move(metrics)) {
        signal->counters = counters;
        if (!signal->changed)
            throw std::runtime_error("Cannot create process-watcher wake event.");
        worker = std::jthread([this](std::stop_token stop) { run(stop); });
    }
    void run(std::stop_token stop) {
        ++counters->workers;
        Apartment apartment;
        ComPointer<IWbemServices> services;
        ComPointer<IWbemObjectSink> sink;
        bool registered = false, ready_reported = false;
        std::stop_callback wake_on_stop(stop, [&] { SetEvent(signal->changed.get()); });
        try {
            require(apartment.result(), "Initialize pending-process watcher MTA");
            ComPointer<IWbemLocator> locator;
            require(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_IWbemLocator, locator.put_void()),
                    "Create pending-process WMI locator");
            BString scope(L"ROOT\\CIMV2");
            require(locator->ConnectServer(scope.get(), nullptr, nullptr, nullptr, 0, nullptr,
                                           nullptr, services.put()),
                    "Connect current-user WMI");
            require(CoSetProxyBlanket(services.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr,
                                      EOAC_NONE),
                    "Set local process-watcher authentication");
            DWORD session = 0;
            if (!ProcessIdToSessionId(GetCurrentProcessId(), &session))
                throw std::runtime_error("Cannot identify this Windows logon session.");
            BString language(L"WQL"),
                query(L"SELECT * FROM Win32_ProcessStartTrace WHERE SessionID = " +
                      std::to_wstring(session));
            *sink.put() = new ProcessSink(signal);
            // No process-global COM security changes, elevation or polling
            // fallback. Unsupported callback permissions remain an explicit
            // pending-start limitation; foreground observation can still retry.
            const auto result = services->ExecNotificationQueryAsync(
                language.get(), query.get(), WBEM_FLAG_SEND_STATUS, nullptr, sink.get());
            require(result, "Subscribe to asynchronous process-start events");
            registered = true;
            counters->process_watching = true;
            started = {true, "process_watch_started",
                       "Current-user process-start notifications retain saved assignments across "
                       "relaunches."};
            started_promise.set_value(started);
            ready_reported = true;
            if (callbacks.diagnostic)
                callbacks.diagnostic(
                    {utc_timestamp(), Severity::Information, started.code, started.detail});
            while (!stop.stop_requested()) {
                WaitForSingleObject(signal->changed.get(), INFINITE);
                if (stop.stop_requested())
                    break;
                std::deque<DWORD> starts;
                std::optional<std::pair<std::vector<ApplicationIdentity>,
                                        std::shared_ptr<std::promise<OperationResult>>>>
                    pending;
                std::optional<HRESULT> failed;
                {
                    std::lock_guard lock(signal->mutex);
                    starts.swap(signal->starts);
                    pending = std::exchange(signal->pending, {});
                    failed = std::exchange(signal->failed, {});
                    ResetEvent(signal->changed.get());
                }
                if (pending) {
                    wanted.clear();
                    for (const auto& identity : pending->first)
                        wanted.insert(identity_key(canonical_file_identity(identity)));
                    pending->second->set_value(
                        {true, "process_watch_configured",
                         "Managed identities registered before the initial process snapshot."});
                }
                if (failed)
                    require(*failed, "Process-start subscription ended unexpectedly");
                for (const auto pid : starts) {
                    Handle process(
                        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid));
                    if (!process || !current_user_process(process.get()))
                        continue;
                    const auto evidence = inspect_process(pid);
                    if (evidence.result.ok && wanted.contains(identity_key(evidence.identity)) &&
                        callbacks.process_started)
                        callbacks.process_started({evidence.identity, evidence.token});
                }
            }
        } catch (const std::exception& error) {
            started = {false, "process_watcher_unavailable", error.what()};
            if (!ready_reported) {
                started_promise.set_value(started);
                ready_reported = true;
            }
            if (callbacks.diagnostic)
                callbacks.diagnostic(
                    {utc_timestamp(), Severity::Warning, started.code, started.detail});
        }
        {
            std::lock_guard lock(signal->mutex);
            signal->accepting = false;
            if (signal->pending) {
                signal->pending->second->set_value(
                    {false, "process_watcher_unavailable",
                     "Process watcher stopped before configuration completed."});
                signal->pending.reset();
            }
            signal->starts.clear();
        }
        if (registered) {
            const auto result = services->CancelAsyncCall(sink.get());
            if (FAILED(result) && started.ok)
                cleanup = native_result(result, "Cancel process-start notification");
        }
        sink.reset();
        services.reset();
        counters->process_watching = false;
        --counters->workers;
    }
};

ProcessWatcher::ProcessWatcher(BackendCallbacks callbacks, std::shared_ptr<NativeCounters> counters)
    : implementation_(std::make_unique<Implementation>(std::move(callbacks), std::move(counters))) {
}
ProcessWatcher::~ProcessWatcher() {
    shutdown();
}
void ProcessWatcher::set_identities(const std::vector<ApplicationIdentity>& identities) {
    const auto started = implementation_->started_future.get();
    if (!started.ok)
        throw std::runtime_error(started.detail);
    auto completion = std::make_shared<std::promise<OperationResult>>();
    auto ready = completion->get_future();
    {
        std::lock_guard lock(implementation_->signal->mutex);
        if (!implementation_->signal->accepting)
            throw std::runtime_error("Process-start notifications are unavailable.");
        if (implementation_->signal->pending)
            implementation_->signal->pending->second->set_value(
                {false, "superseded", "Pending process identities were superseded."});
        implementation_->signal->pending = std::make_pair(identities, completion);
        SetEvent(implementation_->signal->changed.get());
    }
    completion.reset(); // A worker exception must break the future rather than leave it waiting.
    const auto configured = ready.get();
    if (!configured.ok)
        throw std::runtime_error(configured.detail);
}
OperationResult ProcessWatcher::shutdown() {
    if (implementation_->worker.joinable()) {
        implementation_->worker.request_stop();
        SetEvent(implementation_->signal->changed.get());
        implementation_->worker.join();
    }
    return implementation_->cleanup;
}

} // namespace audiohole::plug_board::windows_detail
