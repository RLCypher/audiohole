#include "module_state.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace audiohole::plug_board {
namespace {
std::string identity_key(const ApplicationIdentity& identity) {
    return std::to_string(static_cast<int>(identity.kind)) + ':' + identity.value;
}
} // namespace

void Module::Implementation::mark_flush() {
    if (!flush_due)
        flush_due = Clock::now() + std::chrono::milliseconds(500);
}

void Module::Implementation::log(Severity severity, std::string code, std::string message,
                                 std::string app, Document details, std::string operation) {
    // Logs are bounded and private-path redacted even before reaching a host
    // sink. Full native identities remain available only in an explicit export.
    if (message.size() > 2048) {
        std::size_t boundary = 2048;
        while (boundary > 0 && (static_cast<unsigned char>(message[boundary]) & 0xC0) == 0x80)
            --boundary;
        message.resize(boundary);
        message += "...";
    }
    // OS exception text can arrive in a legacy encoding. Repair invalid UTF-8
    // only at the logging boundary so a diagnostic cannot terminate the worker.
    message =
        Document::parse(Document(message).dump(-1, ' ', false, Document::error_handler_t::replace))
            .get<std::string>();
    if (!details.is_object() ||
        details.dump(-1, ' ', false, Document::error_handler_t::replace).size() > 4096)
        details = {{"details_truncated", true}};
    else
        details = Document::parse(details.dump(-1, ' ', false, Document::error_handler_t::replace));
    DiagnosticEvent event{
        utc_timestamp(), severity, std::move(code), std::move(message), std::move(operation),
        selected,        {},       std::move(app),  std::move(details)};
    const auto redacted = diagnostic_document(event);
    event.message = redacted.at("message").get<std::string>();
    event.details = redacted.at("details");
    if (!diagnostics.empty() && diagnostics.back().code == event.code &&
        diagnostics.back().application_id == event.application_id &&
        diagnostics.back().message == event.message) {
        auto& last = diagnostics.back();
        last.details["repeat_count"] = last.details.value("repeat_count", 1u) + 1;
        last.details["last_utc"] = event.utc;
    } else {
        diagnostics.push_back(event);
        if (diagnostics.size() > 512)
            diagnostics.pop_front();
    }
    diagnostics_dirty = true;
    mark_flush();
    if (options.diagnostic_sink) {
        try {
            options.diagnostic_sink(event);
        } catch (...) {
            options.diagnostic_sink = {};
        }
    }
}

OperationResult Module::Implementation::commit(Library candidate) {
    try {
        const auto bytes = serialize_library(candidate);
        if (!storage)
            return {false, "storage_unavailable", "The library storage is unavailable."};
        const auto result = storage->replace(expected_bytes, bytes);
        storage_result = result;
        if (result.ok) {
            expected_bytes = bytes;
            library = std::make_shared<const Library>(std::move(candidate));
            catalog_dirty = false;
            ++library_commits;
            return result;
        }
        // A flush can fail after an atomic replacement. Adopt actual new bytes
        // as uncertain intent, but never submit policy under this failed commit.
        // A later explicit retry must achieve a confirmed durable replacement.
        const auto actual = storage->read();
        if (actual.result.ok && actual.bytes == bytes) {
            expected_bytes = bytes;
            library = std::make_shared<const Library>(std::move(candidate));
            catalog_dirty = true;
        }
        log(Severity::Error, result.code, result.detail);
        return result;
    } catch (const std::exception& error) {
        storage_result = {false, "library_validation_failed", error.what()};
        log(Severity::Error, storage_result.code, storage_result.detail);
        return storage_result;
    }
}

void Module::Implementation::rebuild_identity_index() {
    identity_index.clear();
    if (!library)
        return;
    std::set<std::string> retained_ids;
    for (const auto& record : library->applications)
        retained_ids.insert(record.id);
    // External library edits can remove records. Retire their runtime caches as
    // well, so repeated manual replacement cannot grow memory without a bound.
    std::erase_if(applications,
                  [&](const auto& entry) { return !retained_ids.contains(entry.first); });
    std::erase_if(images, [&](const auto& entry) { return !retained_ids.contains(entry.first); });
    std::erase_if(image_requests,
                  [&](const auto& entry) { return !retained_ids.contains(entry.first); });
    std::erase_if(provisional_ids,
                  [&](const auto& entry) { return !retained_ids.contains(entry.second); });
    for (const auto& app : library->applications) {
        auto& runtime = applications[app.id];
        const auto signature = catalog_signature(app);
        if (!runtime.identity_signature.empty() && runtime.identity_signature != signature) {
            runtime.processes.clear();
            runtime.observation.reset();
            runtime.fresh = false;
            runtime.attempted_fingerprint.clear();
            runtime.icon_signature.clear();
            images.erase(app.id);
            image_requests.erase(app.id);
            runtime.policy.state = PolicyState::Stale;
            runtime.policy.detail =
                "Identity changed; a new process must be verified before routing.";
            log(Severity::Information, "identity_changed", runtime.policy.detail, app.id);
        }
        runtime.identity_signature = signature;
        auto remember = [&](const ApplicationIdentity& identity) {
            if (identity.kind == IdentityKind::Unresolved)
                return;
            const auto key = identity_key(identity);
            if (!canonical_identity_cache.contains(key))
                canonical_identity_cache[key] =
                    backend ? backend->canonical_identity(identity) : identity;
            const auto canonical = identity_key(canonical_identity_cache.at(key));
            for (const auto& candidate : {key, canonical}) {
                const auto found = identity_index.find(candidate);
                if (found != identity_index.end() && found->second != app.id) {
                    // Preserve both saved records and surface ambiguity rather than
                    // silently merging two assignments through a filesystem alias.
                    identity_index[candidate] = {};
                    log(Severity::Warning, "identity_collision",
                        "Saved application identities resolve to the same executable; "
                        "reassociation "
                        "needs review.",
                        app.id);
                } else
                    identity_index[candidate] = app.id;
            }
        };
        remember(app.identity);
        for (const auto& alias : app.verified_aliases)
            remember(alias);
        applications.try_emplace(app.id);
    }
    std::erase_if(canonical_identity_cache,
                  [&](const auto& entry) { return !identity_index.contains(entry.first); });
}

void Module::Implementation::load_local_state() {
    if (options.memory_only)
        return;
    if (!local_history_loaded) {
        local_history_loaded = true;
        const auto previous_log = read_local_document(options.local_root / "diagnostics.json");
        if (previous_log.result.ok && previous_log.bytes) {
            try {
                const auto document = parse_document(*previous_log.bytes);
                if (document.at("format") != "audiohole-plug-board-event-log" ||
                    document.at("schema_version") != 1 || !document.at("events").is_array() ||
                    document.at("events").size() > 512)
                    throw std::runtime_error("Unrecognized local diagnostic log format.");
                const auto& events = document.at("events");
                const auto begin = events.size() > 128 ? events.size() - 128 : 0;
                for (std::size_t index = begin; index < events.size(); ++index) {
                    const auto& item = events[index];
                    DiagnosticEvent event;
                    event.utc = item.at("utc").get<std::string>();
                    event.code = item.at("code").get<std::string>();
                    event.message = item.at("message").get<std::string>();
                    event.operation_id = item.value("operation_id", "");
                    event.board_id = item.value("board_id", "");
                    event.socket_id = item.value("socket_id", "");
                    event.application_id = item.value("application_id", "");
                    const auto severity = item.at("severity").get<std::string>();
                    event.severity = severity == "error"     ? Severity::Error
                                     : severity == "warning" ? Severity::Warning
                                                             : Severity::Information;
                    event.details = item.value("details", Document::object());
                    event.details["historical"] = true;
                    while (diagnostics.size() >= 512)
                        diagnostics.pop_front();
                    diagnostics.push_back(std::move(event));
                }
            } catch (const std::exception& error) {
                diagnostics_writable = false;
                diagnostic_storage_result = {false, "diagnostic_log_invalid",
                                             "The previous diagnostic log is invalid and was "
                                             "preserved. New diagnostics remain in memory."};
                log(Severity::Error, diagnostic_storage_result.code, error.what());
            }
        } else if (!previous_log.result.ok) {
            diagnostics_writable = false;
            diagnostic_storage_result = previous_log.result;
            log(Severity::Error, previous_log.result.code, previous_log.result.detail);
        }
        const auto history = read_local_document(options.local_root / "history.json");
        if (history.result.ok && history.bytes) {
            try {
                const auto value = parse_document(*history.bytes);
                if (value.at("format") != "audiohole-plug-board-local" ||
                    value.at("schema_version") != 1)
                    throw std::runtime_error("Unknown local history format.");
                const auto now = utc_milliseconds(utc_timestamp()).value();
                for (auto item = value.at("applications").begin();
                     item != value.at("applications").end(); ++item) {
                    if (!find_application(*library, item.key()))
                        continue;
                    auto& runtime = applications[item.key()];
                    if (!item.value().at("last_active_utc_ms").is_null()) {
                        const auto stamp =
                            item.value().at("last_active_utc_ms").get<std::int64_t>();
                        if (stamp <= now && stamp >= 0) {
                            runtime.history.last_active_utc_ms = stamp;
                            runtime.last_active_monotonic =
                                Clock::now() - std::chrono::milliseconds(now - stamp);
                        } else
                            runtime.history.clock_uncertain = true;
                    }
                    runtime.history.last_active_windows_endpoints = item.value().value(
                        "last_active_windows_endpoints", std::vector<std::string>{});
                    if (item.value().contains("last_policy_state")) {
                        runtime.policy.state = PolicyState::Stale;
                        runtime.policy.detail = "Historical policy receipt; current process and "
                                                "preference have not been checked.";
                    }
                }
            } catch (const std::exception& error) {
                history_writable = false;
                log(Severity::Warning, "history_invalid", error.what());
            }
        } else if (!history.result.ok) {
            history_writable = false;
            log(Severity::Warning, history.result.code, history.result.detail);
        }
    }
    if (options.standalone_inventory) {
        if (!device_storage)
            device_storage = make_disk_storage(options.local_root / "devices.json");
        const auto devices = device_storage->read();
        device_bytes = devices.bytes;
        if (!devices.result.ok)
            throw std::runtime_error(devices.result.detail);
        if (devices.bytes) {
            const auto value = parse_document(*devices.bytes);
            if (value.at("format") != "audiohole-plug-board-local-devices" ||
                value.at("schema_version") != 1)
                throw std::runtime_error(
                    "Unsupported local device binding format. The file was preserved.");
            standalone_devices = value.at("bindings").get<std::map<std::string, std::string>>();
            if (standalone_devices.size() > 20000)
                throw std::runtime_error("Local device binding count exceeds its bound.");
            standalone_device_extra = value;
        }
    }
}

void Module::Implementation::flush() {
    if (catalog_dirty && library && storage && commit(*library).ok)
        reconcile(false);
    if (options.memory_only) {
        history_dirty = diagnostics_dirty = false;
        return;
    }
    if (history_dirty && history_writable) {
        Document records = Document::object();
        for (const auto& [id, app] : applications)
            records[id] = {
                {"last_active_utc_ms", app.history.last_active_utc_ms
                                           ? Document(*app.history.last_active_utc_ms)
                                           : Document(nullptr)},
                {"last_active_windows_endpoints", app.history.last_active_windows_endpoints},
                {"last_policy_state", policy_state_name(app.policy.state)},
                {"last_policy_operation", app.policy.operation_id},
                {"historical_receipt_only", true}};
        const Document document{{"format", "audiohole-plug-board-local"},
                                {"schema_version", 1},
                                {"saved_utc", utc_timestamp()},
                                {"applications", std::move(records)}};
        const auto result =
            write_local_document(options.local_root / "history.json", document.dump(2) + '\n');
        if (result.ok)
            history_dirty = false;
        else
            log(Severity::Error, result.code, "Local activity history could not be flushed.");
    }
    if (diagnostics_dirty && diagnostics_writable) {
        Document events = Document::array();
        for (const auto& event : diagnostics)
            events.push_back(diagnostic_document(event));
        const Document document{{"format", "audiohole-plug-board-event-log"},
                                {"schema_version", 1},
                                {"saved_utc", utc_timestamp()},
                                {"bounded_event_count", 512},
                                {"events", std::move(events)}};
        const auto result =
            write_local_document(options.local_root / "diagnostics.json", document.dump(2) + '\n');
        diagnostic_storage_result = result;
        if (result.ok) {
            diagnostics_dirty = false;
            ++diagnostic_flushes;
        }
        // Avoid recursively logging a failure to write the log itself. Its state
        // remains dirty and export can still retrieve the in-memory event ring.
    }
    // A failed flush waits for an explicit command, a new change, or shutdown;
    // it never starts a periodic hidden retry/write loop.
    flush_due.reset();
}

CommandResult Module::Implementation::execute(const Command& command,
                                              std::optional<std::uint64_t> expected,
                                              std::optional<std::string> expected_board) {
    CommandResult result;
    result.operation_id = new_id("operation");
    try {
        if (!library || phase != ModulePhase::Ready)
            throw std::runtime_error("The library is read-only until its load error is repaired.");
        result.revision = library->revision;
        if ((expected && *expected != library->revision) ||
            (expected_board && *expected_board != selected)) {
            result.code = "stale_revision";
            result.message = "The configuration changed; review the latest board and try again.";
        } else {
            auto edit = prepare_edit(*library, selected, command, utc_timestamp());
            const bool needs_commit = edit.changed || catalog_dirty || !storage_result.ok;
            const auto persisted =
                needs_commit ? commit(std::move(edit.library))
                             : OperationResult{true, "already_durable",
                                               "The desired configuration is already durable."};
            if (!persisted.ok) {
                result.code = persisted.code;
                result.message = persisted.detail;
                policy_pending.clear();
                reconcile(false);
            } else {
                selected = edit.selected_board_id;
                result.created_id = edit.created_id;
                result.ok = true;
                result.durable_intent = true;
                result.code = "intent_recorded";
                result.message = "Working changes recorded. Windows preference and playback status "
                                 "are reported separately.";
                rebuild_identity_index();
                if (std::holds_alternative<RefreshApplications>(command)) {
                    image_requests.clear();
                    capability = backend->capability();
                    if (options.standalone_inventory)
                        refresh_standalone_inventory();
                    if (visible)
                        backend->refresh_observation();
                    reconcile(true);
                    result.message = "Refresh requested. Existing assignments were retained.";
                } else if (edit.reconcile) {
                    if (const auto* move = std::get_if<MoveApplication>(&command)) {
                        ++routing_generation;
                        applications[move->application_id].attempted_fingerprint.clear();
                        reconcile(false, move->application_id);
                    } else if (const auto* endpoint = std::get_if<SetSocketEndpoint>(&command)) {
                        ++routing_generation;
                        for (auto& [id, runtime] : applications)
                            if (const auto* socket = socket_for(id);
                                socket && socket->id == endpoint->socket_id)
                                runtime.attempted_fingerprint.clear();
                        reconcile(false);
                    } else {
                        const auto* retry = std::get_if<RetryRouting>(&command);
                        reconcile(true, retry ? retry->application_id : std::string{});
                    }
                }
                result.revision = library->revision;
            }
        }
    } catch (const std::exception& error) {
        result.code = "command_rejected";
        result.message = error.what();
    }
    last_command = result;
    log(result.ok ? Severity::Information : Severity::Error, result.code, result.message, {}, {},
        result.operation_id);
    publish();
    return result;
}

CommandResult Module::Implementation::reload() {
    CommandResult result;
    result.operation_id = new_id("reload");
    try {
        if (!storage)
            storage = options.memory_only ? make_memory_storage()
                                          : options.storage(options.data_root / "library.json");
        const auto loaded = storage->read();
        if (!loaded.result.ok || !loaded.bytes)
            throw std::runtime_error(
                loaded.result.ok ? "The library file is missing; no replacement was created."
                                 : loaded.result.detail);
        auto candidate = parse_library(*loaded.bytes);
        expected_bytes = loaded.bytes;
        library = std::make_shared<const Library>(std::move(candidate));
        if (!find_board(*library, selected))
            selected = library->default_board_id;
        catalog_dirty = false;
        storage_result = {true, "reloaded", "External configuration loaded and validated."};
        canonical_identity_cache.clear();
        rebuild_identity_index();
        if (phase != ModulePhase::Ready)
            load_local_state();
        phase = ModulePhase::Ready;
        if (options.standalone_inventory)
            refresh_standalone_inventory();
        reconcile(true);
        if (visible)
            backend->observe(true, inventory, ++discovery_generation);
        result = {true,
                  true,
                  "reloaded",
                  storage_result.detail,
                  result.operation_id,
                  {},
                  library->revision};
    } catch (const std::exception& error) {
        result.code = "reload_failed";
        result.message = error.what();
    }
    last_command = result;
    log(result.ok ? Severity::Information : Severity::Error, result.code, result.message);
    publish();
    return result;
}

CommandResult Module::Implementation::restore(const std::filesystem::path& backup, bool confirmed) {
    CommandResult result;
    result.operation_id = new_id("restore");
    try {
        if (options.memory_only)
            throw std::runtime_error("The isolated tutorial cannot import a working library.");
        if (!confirmed)
            throw std::runtime_error("Restoring a library requires explicit confirmation.");
        const auto source = read_local_document(backup);
        if (!source.result.ok || !source.bytes)
            throw std::runtime_error("The selected backup cannot be read.");
        auto candidate = parse_library(*source.bytes);
        if (!storage)
            storage = options.storage(options.data_root / "library.json");
        const auto current = storage->read();
        if (!current.result.ok)
            throw std::runtime_error(current.result.detail);
        if (expected_bytes && current.bytes != expected_bytes)
            throw std::runtime_error("The current library changed since the restore was requested; "
                                     "reload before restoring.");
        expected_bytes = current.bytes;
        const auto committed = commit(std::move(candidate));
        if (!committed.ok)
            throw std::runtime_error(committed.detail);
        selected = library->default_board_id;
        if (phase != ModulePhase::Ready)
            load_local_state();
        phase = ModulePhase::Ready;
        canonical_identity_cache.clear();
        rebuild_identity_index();
        if (options.standalone_inventory)
            refresh_standalone_inventory();
        reconcile(true);
        result = {
            true,
            true,
            "restored",
            "The validated backup was restored; previous bytes were retained in backup history.",
            result.operation_id,
            {},
            library->revision};
    } catch (const std::exception& error) {
        result.code = "restore_failed";
        result.message = error.what();
    }
    last_command = result;
    log(result.ok ? Severity::Information : Severity::Error, result.code, result.message);
    publish();
    return result;
}

OperationResult Module::Implementation::export_to(const std::filesystem::path& file, bool sensitive,
                                                  bool overwrite) {
    try {
        if (options.memory_only)
            return {false, "tutorial_isolation",
                    "The isolated tutorial does not export working files."};
        if (std::filesystem::exists(file) && !overwrite)
            return {false, "confirmation_required",
                    "Choose a new export filename or confirm replacement."};
        publish();
        const auto bytes = snapshot_document(*published.load(), sensitive).dump(2) + '\n';
        const auto result = write_local_document(file, bytes);
        log(result.ok ? Severity::Information : Severity::Error, "diagnostic_export",
            result.ok ? "Diagnostic report exported." : result.detail);
        publish();
        return result;
    } catch (const std::exception& error) {
        return {false, "export_failed", error.what()};
    }
}

} // namespace audiohole::plug_board
