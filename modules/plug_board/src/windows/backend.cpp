#include "native_runtime.hpp"

#include <mmdeviceapi.h>
#include <psapi.h>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace audiohole::plug_board {
using namespace windows_detail;
namespace {

struct InventorySignal {
    std::mutex mutex;
    std::condition_variable drained;
    std::function<void()> changed;
    bool accepting = true;
    unsigned active = 0;
    void notify() {
        std::function<void()> callback;
        {
            std::lock_guard lock(mutex);
            if (!accepting)
                return;
            ++active;
            callback = changed;
        }
        try {
            if (callback)
                callback();
        } catch (...) {
        }
        {
            std::lock_guard lock(mutex);
            --active;
            drained.notify_all();
        }
    }
};
class DeviceEvents final : public IMMNotificationClient {
  public:
    explicit DeviceEvents(std::shared_ptr<InventorySignal> signal) : signal_(std::move(signal)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** pointer) override {
        if (!pointer)
            return E_POINTER;
        *pointer = nullptr;
        if (id == IID_IUnknown || id == __uuidof(IMMNotificationClient)) {
            *pointer = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto count = --references_;
        if (!count)
            delete this;
        return count;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
        signal_->notify();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
        signal_->notify();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
        signal_->notify();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole, LPCWSTR) override {
        if (flow == eRender)
            signal_->notify();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
        signal_->notify();
        return S_OK;
    }

  private:
    std::atomic<ULONG> references_{1};
    std::shared_ptr<InventorySignal> signal_;
};

class WindowsBackend final : public PlatformBackend {
  public:
    explicit WindowsBackend(bool inventory_notifications)
        : owner_(std::this_thread::get_id()), watch_inventory_(inventory_notifications),
          environment_(inspect_windows_environment()) {
        std::wstring image(32768, L'\0');
        const auto size =
            GetModuleFileNameW(nullptr, image.data(), static_cast<DWORD>(image.size()));
        if (size && size < image.size()) {
            image.resize(size);
            own_image_ = canonical_file_identity({IdentityKind::Win32Image, utf8(image)}).value;
        }
    }
    ~WindowsBackend() override {
        try {
            shutdown();
        } catch (...) {
        }
    }
    bool simulated() const override {
        return false;
    }
    bool internal_application(const ApplicationIdentity& identity) const override {
        if (is_audiohole_application(identity))
            return true;
        if (identity.kind != IdentityKind::Win32Image || own_image_.empty())
            return false;
        return CompareStringOrdinal(wide(identity.value).c_str(), -1, wide(own_image_).c_str(), -1,
                                    TRUE) == CSTR_EQUAL;
    }
    Document environment() const override {
        return {{"source", environment_.wine ? "wine_supplemental" : "native_windows"},
                {"major", environment_.major},
                {"minor", environment_.minor},
                {"build", environment_.build},
                {"update_build_revision", environment_.update_build_revision
                                              ? Document(*environment_.update_build_revision)
                                              : Document(nullptr)},
                {"elevated", environment_.privilege_known ? Document(environment_.elevated)
                                                          : Document(nullptr)},
                {"workstation", environment_.workstation},
                {"wine", environment_.wine}};
    }
    OperationResult initialize(BackendCallbacks callbacks) override {
        check_thread();
        callbacks_ = std::move(callbacks);
        const auto initialized =
            native_result(apartment_.result(), "Initialize Plug Board control MTA");
        if (!initialized.ok)
            return initialized;
        if (watch_inventory_) {
            try {
                inventory_signal_ = std::make_shared<InventorySignal>();
                inventory_signal_->changed = callbacks_.inventory_changed;
                require(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                         __uuidof(IMMDeviceEnumerator), devices_.put_void()),
                        "Create standalone inventory notification source");
                *device_events_.put() = new DeviceEvents(inventory_signal_);
                const auto registered =
                    devices_->RegisterEndpointNotificationCallback(device_events_.get());
                if (FAILED(registered))
                    return native_result(registered, "Register standalone endpoint notifications");
                inventory_registered_ = true;
            } catch (const std::exception& error) {
                return {false, "inventory_notifications_unavailable", error.what()};
            }
        }
        return initialized;
    }
    OperationResult capability() override {
        check_thread();
        auto adapter = make_windows_policy_adapter({}, PolicyScope::UserAssignment);
        return adapter->capability();
    }
    ApplicationIdentity canonical_identity(const ApplicationIdentity& identity) override {
        check_thread();
        return canonical_file_identity(identity);
    }
    RawEndpointInventory inventory() override {
        check_thread();
        // This call does not enumerate sessions or open streams. It is the
        // standalone host shim; integrated hosts supply their scanner inventory.
        const auto read = inspect_windows_sessions(false);
        if (!read.snapshot.complete)
            throw std::runtime_error(read.snapshot.diagnostics.empty()
                                         ? "Playback inventory is incomplete."
                                         : read.snapshot.diagnostics.front());
        RawEndpointInventory value;
        value.defaults = read.default_endpoints;
        for (const auto& endpoint : read.endpoints)
            value.endpoints.push_back(
                {endpoint.endpoint_id, endpoint.display_name, endpoint.available});
        return value;
    }
    std::array<std::optional<std::string>, 3> default_endpoints() override {
        check_thread();
        std::array<std::optional<std::string>, 3> result{};
        ComPointer<IMMDeviceEnumerator> enumerator;
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                    __uuidof(IMMDeviceEnumerator), enumerator.put_void())))
            return result;
        for (unsigned role = 0; role < 3; ++role) {
            ComPointer<IMMDevice> endpoint;
            if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, static_cast<ERole>(role),
                                                           endpoint.put())))
                continue;
            LPWSTR id = nullptr;
            if (SUCCEEDED(endpoint->GetId(&id)) && id)
                result[role] = utf8(id);
            CoTaskMemFree(id);
        }
        return result;
    }
    void observe(bool visible, const EndpointInventory& inventory,
                 std::uint64_t generation) override {
        check_thread();
        current_inventory_ = inventory;
        current_generation_ = generation;
        if (!visible) {
            if (sessions_) {
                const auto stopped = sessions_->shutdown();
                if (!stopped.ok)
                    diagnostic(Severity::Warning, stopped.code, stopped.detail);
                sessions_.reset();
            }
            if (icons_)
                icons_->cancel_pending();
        } else {
            if (!sessions_)
                sessions_ = std::make_unique<SessionMonitor>(callbacks_, counters_);
            sessions_->configure(inventory, generation);
        }
    }
    void refresh_observation() override {
        check_thread();
        if (icons_)
            icons_->invalidate_sources();
        if (!sessions_)
            return;
        if (!counters_->observing) {
            sessions_->shutdown();
            sessions_ = std::make_unique<SessionMonitor>(callbacks_, counters_);
            sessions_->configure(current_inventory_, current_generation_);
        } else
            sessions_->refresh();
    }
    std::vector<ProcessEvidence>
    find_processes(const std::vector<ApplicationIdentity>& identities) override {
        check_thread();
        // The module installs its complete managed-identity subscription before
        // requesting snapshots. A subset lookup must not replace that watch.
        ++counters_->process_snapshots;
        return find_current_user_processes(identities);
    }
    void watch_processes(const std::vector<ApplicationIdentity>& identities) override {
        check_thread();
        if (identities.empty()) {
            if (processes_) {
                const auto stopped = processes_->shutdown();
                if (!stopped.ok)
                    diagnostic(Severity::Warning, stopped.code, stopped.detail);
                processes_.reset();
            }
            return;
        }
        if (environment_.wine)
            return;
        try {
            if (!processes_)
                processes_ = std::make_unique<ProcessWatcher>(callbacks_, counters_);
            processes_->set_identities(identities);
        } catch (const std::exception& error) {
            diagnostic(Severity::Warning, "process_watcher_unavailable", error.what());
            if (processes_) {
                processes_->shutdown();
                processes_.reset();
            }
        }
    }
    void set_icon_cache_root(const std::filesystem::path& root) override {
        check_thread();
        icon_cache_root_ = root;
    }
    void request_icons(const std::vector<ApplicationObservation>& applications, unsigned pixels,
                       std::uint64_t generation) override {
        check_thread();
        if (!icons_)
            icons_ = std::make_unique<IconWorker>(callbacks_, counters_, icon_cache_root_);
        icons_->request(applications, pixels, generation);
    }
    LivePolicyReport apply_policy(const std::vector<ProcessToken>& tokens,
                                  const std::optional<std::string>& target,
                                  std::uint64_t generation, std::stop_token stop) override {
        check_thread();
        LivePolicyReport result;
        result.generation = generation;
        result.target_windows_id = target;
        result.operation_id = new_id("policy");
        bool native_write = false;
        auto adapter =
            make_windows_policy_adapter(tokens, PolicyScope::UserAssignment, [&](bool write) {
                if (write) {
                    ++counters_->policy_writes;
                    native_write = true;
                } else
                    ++counters_->policy_reads;
            });
        const auto available = adapter->capability();
        if (!available.ok) {
            result.state = PolicyState::Unsupported;
            result.detail = available.detail;
            return result;
        }
        if (tokens.empty()) {
            result.state = PolicyState::PendingProcess;
            result.detail = "A verified live application process is required.";
            return result;
        }
        std::size_t accepted = 0;
        bool process_gone = false;
        for (const auto& process : tokens)
            for (const auto role : render_roles) {
                if (stop.stop_requested()) {
                    result.state = PolicyState::Partial;
                    result.detail = "Preference work was superseded or stopped; completed role "
                                    "results remain in diagnostics.";
                    return result;
                }
                RolePolicyResult row;
                row.process = process;
                row.role = role;
                row.before = adapter->read(process, role);
                if (row.before.result.ok) {
                    if (row.before.endpoint_id == target) {
                        row.write = {true, "already_requested",
                                     "This role already uses the requested preference."};
                        row.readback = row.before;
                    } else if (!stop.stop_requested()) {
                        native_write = false;
                        row.write = adapter->write(process, role, target);
                        row.write_attempted = native_write;
                        row.readback = adapter->read(process, role);
                    } else {
                        row.write = {false, "cancelled",
                                     "A newer request cancelled this role before its setter."};
                    }
                } else {
                    row.write = {
                        false, "read_required",
                        "The original role preference could not be read; no setter was called."};
                    row.readback = row.before;
                }
                if (row.before.result.code == "process_not_live" ||
                    row.before.result.code == "process_identity_changed" ||
                    row.readback.result.code == "process_not_live" ||
                    row.readback.result.code == "process_identity_changed")
                    process_gone = true;
                row.accepted = row.before.result.ok && row.write.ok && row.readback.result.ok &&
                               row.readback.endpoint_id == target;
                if (row.accepted)
                    ++accepted;
                result.roles.push_back(std::move(row));
            }
        result.state = accepted == tokens.size() * render_roles.size() ? PolicyState::Accepted
                       : accepted                                      ? PolicyState::Partial
                       : process_gone                                  ? PolicyState::PendingProcess
                                                                       : PolicyState::Rejected;
        result.detail =
            result.state == PolicyState::Accepted
                ? "All verified process and playback-role preferences were accepted. Active stream "
                  "destinations are observed independently."
            : result.state == PolicyState::PendingProcess
                ? "The application process changed or exited; desired routing remains saved."
                : "One or more process or playback-role preferences were not accepted. Inspect the "
                  "per-role native results.";
        return result;
    }
    PlatformMetrics metrics(bool sample) override {
        check_thread();
        if (sample) {
            // Timestamp the measurement itself. Other service publications may
            // reuse it; views must not treat those as additional CPU samples.
            measured_.process_sample_tick_ms = GetTickCount64();
            measured_.process_cpu_100ns.reset();
            measured_.private_bytes.reset();
            measured_.working_set_bytes.reset();
            measured_.handles.reset();
            measured_.io_write_operations.reset();
            FILETIME created{}, exited{}, kernel{}, user{};
            if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
                measured_.process_cpu_100ns = file_time(kernel) + file_time(user);
            PROCESS_MEMORY_COUNTERS_EX memory{};
            memory.cb = sizeof(memory);
            if (GetProcessMemoryInfo(GetCurrentProcess(),
                                     reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                                     sizeof(memory))) {
                measured_.private_bytes = memory.PrivateUsage;
                measured_.working_set_bytes = memory.WorkingSetSize;
            }
            DWORD handles = 0;
            if (GetProcessHandleCount(GetCurrentProcess(), &handles))
                measured_.handles = handles;
            IO_COUNTERS io{};
            if (GetProcessIoCounters(GetCurrentProcess(), &io))
                measured_.io_write_operations = io.WriteOperationCount;
        }
        auto result = measured_;
        result.full_session_enumerations = counters_->full_enumerations;
        result.session_events = counters_->session_events;
        result.icon_extractions = counters_->icon_extractions;
        result.icon_cache_hits = counters_->icon_cache_hits;
        result.policy_reads = counters_->policy_reads;
        result.policy_writes = counters_->policy_writes;
        result.process_snapshots = counters_->process_snapshots;
        result.dropped_events = counters_->dropped;
        result.owned_workers = counters_->workers;
        result.discovery_active = counters_->observing;
        result.process_watch_active = counters_->process_watching;
        return result;
    }
    void shutdown() override {
        check_thread();
        if (stopped_)
            return;
        stopped_ = true;
        OperationResult released{true, "stopped", {}};
        if (sessions_) {
            const auto result = sessions_->shutdown();
            if (!result.ok)
                released = result;
            sessions_.reset();
        }
        if (processes_) {
            const auto result = processes_->shutdown();
            if (!result.ok)
                released = result;
            processes_.reset();
        }
        if (icons_) {
            icons_->shutdown();
            icons_.reset();
        }
        if (inventory_signal_) {
            std::lock_guard lock(inventory_signal_->mutex);
            inventory_signal_->accepting = false;
        }
        if (devices_ && inventory_registered_) {
            const auto result =
                devices_->UnregisterEndpointNotificationCallback(device_events_.get());
            if (FAILED(result))
                released = native_result(result, "Unregister endpoint inventory notification");
        }
        if (inventory_signal_) {
            std::unique_lock lock(inventory_signal_->mutex);
            inventory_signal_->drained.wait(lock, [&] { return inventory_signal_->active == 0; });
            inventory_signal_->changed = {};
        }
        device_events_.reset();
        devices_.reset();
        callbacks_ = {};
        if (!released.ok)
            throw std::runtime_error(released.detail);
    }

  private:
    // Destruction releases every interface/worker before the owning MTA ends.
    std::string own_image_;
    Apartment apartment_;
    std::thread::id owner_;
    bool watch_inventory_ = false;
    bool inventory_registered_ = false;
    bool stopped_ = false;
    WindowsEnvironment environment_;
    BackendCallbacks callbacks_;
    std::shared_ptr<NativeCounters> counters_ = std::make_shared<NativeCounters>();
    std::shared_ptr<InventorySignal> inventory_signal_;
    ComPointer<IMMDeviceEnumerator> devices_;
    ComPointer<IMMNotificationClient> device_events_;
    std::unique_ptr<SessionMonitor> sessions_;
    std::unique_ptr<IconWorker> icons_;
    std::filesystem::path icon_cache_root_;
    std::unique_ptr<ProcessWatcher> processes_;
    EndpointInventory current_inventory_;
    std::uint64_t current_generation_ = 0;
    PlatformMetrics measured_;
    void check_thread() const {
        if (std::this_thread::get_id() != owner_)
            throw std::logic_error("Native backend belongs to its constructing control thread.");
    }
    void diagnostic(Severity severity, const std::string& code, const std::string& message) {
        if (callbacks_.diagnostic)
            callbacks_.diagnostic({utc_timestamp(), severity, code, message});
    }
};
} // namespace

std::unique_ptr<PlatformBackend> make_windows_backend(bool inventory_notifications) {
    return std::make_unique<WindowsBackend>(inventory_notifications);
}

} // namespace audiohole::plug_board
