#include "presentation/scan_progress.h"
#include <algorithm>
namespace audiohole::presentation {
namespace {
std::string string(const scanner::Json& value, const char* key, std::string fallback = {}) {
    return value.contains(key) && value[key].is_string() ? value[key].get<std::string>() : fallback;
}
std::string format(const scanner::Json& value) {
    if (!value.is_object())
        return {};
    auto number = [&](const char* key) {
        return value.contains(key) ? value[key].dump() : "unknown";
    };
    return number("sample_rate_hz") + " Hz / " + number("channel_count") + " ch / " +
           string(value, "sample_format", "unknown format");
}
} // namespace
int ScanProgressText::percent() const {
    return planned_ ? static_cast<int>(std::min<std::size_t>(99, done_ * 100 / planned_)) : -1;
}
std::string ScanProgressText::describe(const scanner::Diagnostic& diagnostic) const {
    auto result = diagnostic.code + ": " + diagnostic.message;
    if (!diagnostic.native_code.empty())
        result += " (" + diagnostic.native_code + ")";
    if (!diagnostic.context.empty()) {
        const auto name = names_.find(diagnostic.context);
        result += name == names_.end() ? " [device identity/details in scan bundle]"
                                       : " [" + name->second + "]";
    }
    return result;
}
std::string ScanProgressText::consume(const scanner::Progress& event) {
    const auto& details = event.details;
    const auto& phase = event.phase;
    if (phase == "scope_selection") {
        names_[event.endpoint_id] = string(details, "device_name", "Unnamed device");
        if (string(details, "direction") == "capture")
            names_[event.endpoint_id] += " · Capture";
        if (string(details, "direction") == "render")
            names_[event.endpoint_id] += " · Playback";
        return "Discovery " + std::to_string(event.completed) + ": " + names_[event.endpoint_id] +
               " — " + string(details, "state", "unknown") +
               (string(details, "capability_queries") == "selected"
                    ? "; selected for Basic queries"
                    : "; retained without retesting");
    }
    if (phase == "deep_plan") {
        planned_ = event.total;
        devices_ = details.value("devices", std::size_t{});
        return "Deep plan: " + std::to_string(devices_) + " eligible devices; " +
               std::to_string(planned_) + " configurations";
    }
    if (phase == "deep_device_started") {
        device_ = event.completed + 1;
        devices_ = event.total;
        names_[event.endpoint_id] =
            string(details, "display_name", "Unnamed device") +
            (string(details, "direction") == "capture" ? " · Capture" : " · Playback");
        const auto logical_id = string(details, "logical_id");
        if (!logical_id.empty())
            names_[logical_id] = names_[event.endpoint_id];
        configuration_.clear();
    }
    std::string prefix;
    if (devices_ && phase.starts_with("deep_"))
        prefix = "Device " + std::to_string(device_) + "/" + std::to_string(devices_) + " | ";
    if (!event.endpoint_id.empty()) {
        const auto known = names_.find(event.endpoint_id);
        prefix += (known == names_.end() ? "Device being identified" : known->second) + " | ";
    }
    if (phase == "deep_attempt_started" || phase == "deep_attempt_preparing") {
        configuration_ = string(details, "mode") + " / " +
                         format(details.value("format", scanner::Json::object()));
        return prefix + "Configuration " + std::to_string(event.completed + 1) + "/" +
               std::to_string(event.total) + " | " + configuration_ +
               (phase == "deep_attempt_preparing" ? " | Saving start checkpoint" : " | Setting up");
    }
    if (phase == "deep_stage") {
        const auto api = string(details, "api");
        const auto stage = api == "BufferProcessing"           ? "Streaming"
                           : api == "Cleanup" || api == "Stop" ? "Cleaning up"
                                                               : "Setting up";
        return prefix + "Configuration " + std::to_string(event.completed + 1) + "/" +
               std::to_string(event.total) + " | " + configuration_ + " | " + stage + " — " + api +
               " " + string(details, "phase");
    }
    if (phase == "deep_attempt_finished") {
        ++done_;
        auto outcome = string(details, "classification");
        if (string(details, "support") == "unsupported")
            outcome = string(details, "hresult") == "0x00000001"
                          ? "closest format offered; exact candidate not accepted"
                          : "candidate format unsupported";
        else if (outcome == "verified")
            outcome = "short stream completed (limited evidence)";
        else if (string(details, "hresult") == "0x8889000A")
            outcome = "device is in use; configuration remains unverified";
        const auto reason = string(details, "reason");
        if (!reason.empty())
            outcome += " — " + reason;
        return prefix + "Configuration " + std::to_string(event.completed) + "/" +
               std::to_string(event.total) + " | " + configuration_ + " | " + outcome +
               " | Overall " + std::to_string(done_) + "/" + std::to_string(planned_);
    }
    if (phase == "deep_checkpoint_finished")
        return prefix + "Saving evidence: " + std::to_string(details.value("checkpoint_ms", 0LL)) +
               " ms; total attempt: " + std::to_string(details.value("attempt_wall_ms", 0LL)) +
               " ms";
    if (phase == "deep_checkpoint_begin")
        return prefix + "Saving final evidence";
    if (phase == "deep_summary_refresh")
        return prefix + "Updating device summaries" +
               (string(details, "phase") == "end"
                    ? " — " + std::to_string(details.value("elapsed_ms", 0LL)) + " ms"
                    : "");
    if (phase == "deep_device_started")
        return prefix + "Deep tests started";
    if (phase == "deep_device_finished")
        return prefix + "Deep tests " + string(details, "result", "finished");
    if (phase == "format_query_begin" || phase == "format_query_end")
        return prefix + "Basic format query " +
               std::to_string(event.completed + (phase == "format_query_begin" ? 1 : 0)) +
               (phase == "format_query_end" ? " finished" : " started") + " | " +
               string(details, "mode");
    if (phase == "api_begin")
        return prefix + string(details, "api", "Reading Windows device information");
    return prefix + (phase == "enumerating"
                         ? "Discovery observation " + std::to_string(event.completed) + "/" +
                               std::to_string(event.total)
                         : "Basic observation recorded");
}
} // namespace audiohole::presentation
