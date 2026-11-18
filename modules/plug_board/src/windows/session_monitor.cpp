#include "native_runtime.hpp"

#include <audiopolicy.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace audiohole::plug_board::windows_detail {
namespace {
// Repeated native failures remain bounded even if a session changes metadata
// continuously. The snapshot is already marked incomplete by each caller.
void note(SessionSnapshot& status, std::string message) {
    if (status.diagnostics.size() < 64 &&
        std::find(status.diagnostics.begin(), status.diagnostics.end(), message) ==
            status.diagnostics.end())
        status.diagnostics.push_back(std::move(message));
}

enum class EventKind { Created, State, Metadata, Disconnected };
struct SessionEvent {
    EventKind kind = EventKind::State;
    std::string endpoint;
    std::string key;
    AudioSessionState state = AudioSessionStateInactive;
    ComPointer<IAudioSessionControl> created;
    std::uint64_t epoch = 0;
};
struct MonitorSignal {
    std::mutex mutex;
    Handle changed{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    std::deque<SessionEvent> events;
    std::optional<std::pair<std::vector<std::string>, std::uint64_t>> configuration;
    bool resynchronize = false;
    bool gap = false;
    bool accepting = true;
    std::shared_ptr<NativeCounters> counters;
    void push(SessionEvent event) {
        std::lock_guard lock(mutex);
        if (!accepting)
            return;
        ++counters->session_events;
        if (events.size() >= 512) {
            gap = true;
            ++counters->dropped;
        } else
            events.push_back(std::move(event));
        SetEvent(changed.get());
    }
};

class SessionEvents final : public IAudioSessionEvents {
  public:
    SessionEvents(std::shared_ptr<MonitorSignal> signal, std::string key, std::uint64_t epoch)
        : signal_(std::move(signal)), key_(std::move(key)), epoch_(epoch) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** pointer) override {
        if (!pointer)
            return E_POINTER;
        *pointer = nullptr;
        if (id == IID_IUnknown || id == IID_IAudioSessionEvents) {
            *pointer = static_cast<IAudioSessionEvents*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto remaining = --references_;
        if (!remaining)
            delete this;
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR, LPCGUID) override {
        changed(EventKind::Metadata);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR, LPCGUID) override {
        changed(EventKind::Metadata);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float, BOOL, LPCGUID) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD, float[], DWORD, LPCGUID) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID, LPCGUID) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState state) override {
        SessionEvent event;
        event.kind = EventKind::State;
        event.key = key_;
        event.state = state;
        event.epoch = epoch_;
        signal_->push(std::move(event));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason) override {
        changed(EventKind::Disconnected);
        return S_OK;
    }

  private:
    std::atomic<ULONG> references_{1};
    std::shared_ptr<MonitorSignal> signal_;
    std::string key_;
    std::uint64_t epoch_;
    void changed(EventKind kind) {
        SessionEvent event;
        event.kind = kind;
        event.key = key_;
        event.epoch = epoch_;
        signal_->push(std::move(event));
    }
};

class SessionCreated final : public IAudioSessionNotification {
  public:
    SessionCreated(std::shared_ptr<MonitorSignal> signal, std::string endpoint, std::uint64_t epoch)
        : signal_(std::move(signal)), endpoint_(std::move(endpoint)), epoch_(epoch) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** pointer) override {
        if (!pointer)
            return E_POINTER;
        *pointer = nullptr;
        if (id == IID_IUnknown || id == IID_IAudioSessionNotification) {
            *pointer = static_cast<IAudioSessionNotification*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto remaining = --references_;
        if (!remaining)
            delete this;
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl* control) override {
        // MTA callback -> same-process MTA worker. Retain the owned interface
        // until consumed; no pointer is published to the GUI or portable model.
        if (control) {
            SessionEvent event;
            event.kind = EventKind::Created;
            event.endpoint = endpoint_;
            event.epoch = epoch_;
            control->AddRef();
            *event.created.put() = control;
            signal_->push(std::move(event));
        }
        return S_OK;
    }

  private:
    std::atomic<ULONG> references_{1};
    std::shared_ptr<MonitorSignal> signal_;
    std::string endpoint_;
    std::uint64_t epoch_;
};

SessionObservation read_session(IAudioSessionControl* session, const std::string& endpoint,
                                SessionSnapshot& status) {
    ComPointer<IAudioSessionControl2> extended;
    require(session->QueryInterface(IID_IAudioSessionControl2, extended.put_void()),
            "Query monitored audio session");
    SessionObservation result;
    result.endpoint_id = endpoint;
    TaskString instance, identifier, display, icon;
    require(extended->GetSessionInstanceIdentifier(instance.put()),
            "Read monitored session instance");
    result.instance_id = utf8(instance.get());
    if (SUCCEEDED(extended->GetSessionIdentifier(identifier.put())))
        result.session_id = utf8(identifier.get());
    if (SUCCEEDED(session->GetDisplayName(display.put())))
        result.display_name = utf8(display.get());
    if (SUCCEEDED(session->GetIconPath(icon.put())))
        result.icon_path = utf8(icon.get());
    AudioSessionState state = AudioSessionStateInactive;
    const auto state_result = session->GetState(&state);
    result.state = FAILED(state_result)                ? SessionState::Unknown
                   : state == AudioSessionStateActive  ? SessionState::Active
                   : state == AudioSessionStateExpired ? SessionState::Expired
                                                       : SessionState::Inactive;
    if (FAILED(state_result)) {
        status.complete = false;
        note(status, native_result(state_result, "Read monitored session state").code);
    }
    const auto system = extended->IsSystemSoundsSession();
    if (system == S_OK) {
        result.identity = {IdentityKind::SystemSounds, "windows-system-sounds"};
        result.display_name = "System Sounds";
    } else {
        DWORD pid = 0;
        const auto process_result = extended->GetProcessId(&pid);
        result.cross_process = process_result == static_cast<HRESULT>(0x0889000D);
        if (SUCCEEDED(process_result) && pid != 0) {
            const auto metadata = inspect_process(pid);
            if (metadata.result.ok) {
                result.identity = metadata.identity;
                result.process = metadata.token;
                if (result.display_name.empty())
                    result.display_name = metadata.display_name;
            } else {
                status.complete = false;
                note(status, metadata.result.code + ": " + metadata.result.detail);
            }
        } else {
            status.complete = false;
            note(status, native_result(process_result, "Read monitored session process").code);
        }
        if (FAILED(system))
            status.complete = false;
    }
    if (result.display_name.empty())
        result.display_name = "Unidentified application";
    return result;
}
} // namespace

struct SessionMonitor::Implementation {
    struct EndpointSubscription {
        ComPointer<IAudioSessionManager2> manager;
        ComPointer<IAudioSessionNotification> sink;
    };
    struct SessionSubscription {
        ComPointer<IAudioSessionControl> control;
        ComPointer<IAudioSessionEvents> sink;
        SessionObservation observation;
        bool registered = false;
    };
    BackendCallbacks callbacks;
    std::shared_ptr<NativeCounters> counters;
    std::shared_ptr<MonitorSignal> signal = std::make_shared<MonitorSignal>();
    std::map<std::string, EndpointSubscription> endpoints;
    std::map<std::string, SessionSubscription> sessions;
    std::deque<SessionObservation> expired;
    SessionSnapshot status;
    std::uint64_t generation = 0;
    std::uint64_t epoch = 0;
    std::vector<std::string> configured;
    OperationResult cleanup{true, "monitor_stopped", "Session subscriptions released."};
    std::jthread worker;

    Implementation(BackendCallbacks value, std::shared_ptr<NativeCounters> metrics)
        : callbacks(std::move(value)), counters(std::move(metrics)) {
        signal->counters = counters;
        if (!signal->changed)
            throw std::runtime_error("Cannot create session-monitor wake event.");
        worker = std::jthread([this](std::stop_token stop) { run(stop); });
    }
    void diagnostic(const std::string& code, const std::string& message) {
        if (callbacks.diagnostic)
            callbacks.diagnostic({utc_timestamp(), Severity::Warning, code, message});
    }
    void release_all() {
        for (auto& [key, session] : sessions) {
            const auto result =
                session.registered
                    ? session.control->UnregisterAudioSessionNotification(session.sink.get())
                    : S_OK;
            if (FAILED(result))
                cleanup = native_result(result, "Unregister session-state notification");
        }
        sessions.clear();
        for (auto& [key, endpoint] : endpoints) {
            const auto result =
                endpoint.manager->UnregisterSessionNotification(endpoint.sink.get());
            if (FAILED(result))
                cleanup = native_result(result, "Unregister session-creation notification");
        }
        endpoints.clear();
        expired.clear();
    }
    void attach(IAudioSessionControl* control, const std::string& endpoint) {
        auto observation = read_session(control, endpoint, status);
        const auto key = endpoint + '\n' + observation.instance_id;
        if (sessions.contains(key)) {
            sessions.at(key).observation = std::move(observation);
            return;
        }
        if (sessions.size() >= 20000) {
            status.complete = false;
            note(status, "Visible session count exceeded its bound.");
            return;
        }
        SessionSubscription subscription;
        control->AddRef();
        *subscription.control.put() = control;
        *subscription.sink.put() = new SessionEvents(signal, key, epoch);
        const auto registered = control->RegisterAudioSessionNotification(subscription.sink.get());
        subscription.registered = SUCCEEDED(registered);
        if (FAILED(registered)) {
            observation.state = SessionState::Unknown;
            status.complete = false;
            note(status, native_result(registered, "Register session-state notification").code);
        }
        subscription.observation = std::move(observation);
        sessions.emplace(key, std::move(subscription));
    }
    void enumerate(IMMDeviceEnumerator* enumerator, std::stop_token stop) {
        release_all();
        status = {};
        ++epoch;
        ++counters->full_enumerations;
        for (const auto& id : configured) {
            if (stop.stop_requested())
                return;
            try {
                ComPointer<IMMDevice> device;
                require(enumerator->GetDevice(wide(id).c_str(), device.put()),
                        "Resolve host playback endpoint");
                EndpointSubscription subscription;
                require(device->Activate(IID_IAudioSessionManager2, CLSCTX_ALL, nullptr,
                                         subscription.manager.put_void()),
                        "Activate visible session manager");
                ComPointer<IAudioSessionEnumerator> initial;
                require(subscription.manager->GetSessionEnumerator(initial.put()),
                        "Prime session notifications");
                int count = 0;
                require(initial->GetCount(&count), "Prime session notification count");
                *subscription.sink.put() = new SessionCreated(signal, id, epoch);
                require(subscription.manager->RegisterSessionNotification(subscription.sink.get()),
                        "Register session creation");
                endpoints.emplace(id, std::move(subscription));
                // Re-enumerate after registration; duplicate creation callbacks
                // are reconciled by endpoint and session-instance identity.
                ComPointer<IAudioSessionEnumerator> current;
                require(endpoints.at(id).manager->GetSessionEnumerator(current.put()),
                        "Read initial visible sessions");
                require(current->GetCount(&count), "Read visible session count");
                if (count < 0 || count > 20000)
                    throw std::runtime_error("Native session enumeration exceeded its bound.");
                for (int index = 0; index < count; ++index) {
                    if (stop.stop_requested())
                        return;
                    ComPointer<IAudioSessionControl> control;
                    require(current->GetSession(index, control.put()), "Read initial session");
                    try {
                        attach(control.get(), id);
                    } catch (const std::exception& error) {
                        status.complete = false;
                        note(status, error.what());
                    }
                }
            } catch (const std::exception& error) {
                status.complete = false;
                note(status, error.what());
            }
        }
    }
    void publish() {
        SessionSnapshot snapshot = status;
        for (const auto& [key, session] : sessions)
            snapshot.sessions.push_back(session.observation);
        snapshot.sessions.insert(snapshot.sessions.end(), expired.begin(), expired.end());
        if (callbacks.observations)
            callbacks.observations(generation, std::move(snapshot), utc_timestamp());
    }
    void consume(SessionEvent event) {
        if (event.epoch != epoch)
            return;
        if (event.kind == EventKind::Created) {
            if (endpoints.contains(event.endpoint))
                attach(event.created.get(), event.endpoint);
            return;
        }
        const auto found = sessions.find(event.key);
        if (found == sessions.end())
            return;
        auto& session = found->second;
        if (event.kind == EventKind::State) {
            session.observation.state =
                event.state == AudioSessionStateActive    ? SessionState::Active
                : event.state == AudioSessionStateExpired ? SessionState::Expired
                                                          : SessionState::Inactive;
            // Publish each Active transition before coalescing subsequent state
            // changes, allowing the portable inbox to retain activity history.
            if (event.state == AudioSessionStateActive)
                publish();
            AudioSessionState current = AudioSessionStateInactive;
            const auto queried = session.control->GetState(&current);
            session.observation.state = FAILED(queried)                      ? SessionState::Unknown
                                        : current == AudioSessionStateActive ? SessionState::Active
                                        : current == AudioSessionStateExpired
                                            ? SessionState::Expired
                                            : SessionState::Inactive;
            if (FAILED(queried))
                status.complete = false;
        } else if (event.kind == EventKind::Metadata) {
            TaskString name, icon;
            if (SUCCEEDED(session.control->GetDisplayName(name.put())) && *name.get())
                session.observation.display_name = utf8(name.get());
            if (SUCCEEDED(session.control->GetIconPath(icon.put())))
                session.observation.icon_path = utf8(icon.get());
        }
        if (event.kind == EventKind::Disconnected ||
            session.observation.state == SessionState::Expired) {
            session.observation.state = SessionState::Expired;
            expired.push_back(session.observation);
            if (expired.size() > 1024)
                expired.pop_front();
            const auto unregistered =
                session.registered
                    ? session.control->UnregisterAudioSessionNotification(session.sink.get())
                    : S_OK;
            if (FAILED(unregistered))
                cleanup = native_result(unregistered, "Release disconnected session notification");
            sessions.erase(found);
        }
    }
    void run(std::stop_token stop) {
        ++counters->workers;
        counters->observing = true;
        std::stop_callback wake_on_stop(stop, [&] { SetEvent(signal->changed.get()); });
        Apartment apartment;
        try {
            require(apartment.result(), "Initialize visible session MTA");
            ComPointer<IMMDeviceEnumerator> enumerator;
            require(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                     IID_IMMDeviceEnumerator, enumerator.put_void()),
                    "Create visible-session endpoint resolver");
            while (!stop.stop_requested()) {
                WaitForSingleObject(signal->changed.get(), INFINITE);
                if (stop.stop_requested())
                    break;
                std::deque<SessionEvent> events;
                std::optional<std::pair<std::vector<std::string>, std::uint64_t>> configuration;
                bool refresh = false, gap = false;
                {
                    std::lock_guard lock(signal->mutex);
                    events.swap(signal->events);
                    configuration = std::exchange(signal->configuration, {});
                    refresh = std::exchange(signal->resynchronize, false);
                    gap = std::exchange(signal->gap, false);
                    ResetEvent(signal->changed.get());
                }
                if (configuration) {
                    configured = std::move(configuration->first);
                    generation = configuration->second;
                    refresh = true;
                }
                if (gap) {
                    diagnostic("native_session_gap", "Session callbacks exceeded their bound; one "
                                                     "visible resynchronization was requested.");
                    refresh = true;
                }
                if (refresh)
                    enumerate(enumerator.get(), stop);
                for (auto& event : events) {
                    try {
                        consume(std::move(event));
                    } catch (const std::exception& error) {
                        status.complete = false;
                        note(status, error.what());
                    }
                }
                publish();
            }
            {
                std::lock_guard lock(signal->mutex);
                signal->accepting = false;
                signal->events.clear();
            }
            release_all();
        } catch (const std::exception& error) {
            cleanup = {false, "session_monitor_failed", error.what()};
            if (callbacks.observations)
                callbacks.observations(generation, {{}, false, {error.what()}}, utc_timestamp());
            diagnostic(cleanup.code, cleanup.detail);
            {
                std::lock_guard lock(signal->mutex);
                signal->accepting = false;
                signal->events.clear();
            }
            release_all();
        }
        counters->observing = false;
        --counters->workers;
    }
};

SessionMonitor::SessionMonitor(BackendCallbacks callbacks, std::shared_ptr<NativeCounters> counters)
    : implementation_(std::make_unique<Implementation>(std::move(callbacks), std::move(counters))) {
}
SessionMonitor::~SessionMonitor() {
    shutdown();
}
void SessionMonitor::configure(const EndpointInventory& inventory, std::uint64_t generation) {
    std::vector<std::string> ids;
    for (const auto& endpoint : inventory.endpoints)
        if (endpoint.playback && endpoint.available && !endpoint.ambiguous &&
            !endpoint.windows_id.empty())
            ids.push_back(endpoint.windows_id);
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    std::lock_guard lock(implementation_->signal->mutex);
    implementation_->signal->configuration = std::make_pair(std::move(ids), generation);
    SetEvent(implementation_->signal->changed.get());
}
void SessionMonitor::refresh() {
    std::lock_guard lock(implementation_->signal->mutex);
    implementation_->signal->resynchronize = true;
    SetEvent(implementation_->signal->changed.get());
}
OperationResult SessionMonitor::shutdown() {
    if (implementation_->worker.joinable()) {
        implementation_->worker.request_stop();
        SetEvent(implementation_->signal->changed.get());
        implementation_->worker.join();
    }
    return implementation_->cleanup;
}

} // namespace audiohole::plug_board::windows_detail
