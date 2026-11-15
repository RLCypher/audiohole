#include "model/latency_preferences.h"
#include <charconv>
#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
namespace audiohole {
TimingValues timing_values(const Preferences& p) {
    return {p.queue_target_ms,
            p.destination.render_ahead_ms,
            p.common.period_ms,
            p.common.buffer_ms ? std::optional<double>(*p.common.buffer_ms) : std::nullopt,
            p.source.period_ms,
            p.destination.period_ms,
            p.source.buffer_ms ? std::optional<double>(*p.source.buffer_ms) : std::nullopt,
            p.destination.buffer_ms ? std::optional<double>(*p.destination.buffer_ms)
                                    : std::nullopt};
}
Result set_timing_values(Preferences& p, const TimingValues& values) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (!values[i])
            continue;
        const double n = *values[i];
        if (!std::isfinite(n) || n <= 0)
            return {
                false,
                "Timing values must be positive finite milliseconds, or empty for Auto/Inherit."};
        if ((i == 3 || i >= 6) && (n > 100 || std::floor(n) != n))
            return {false, "Device buffers require whole milliseconds from 1 to 100, or empty for "
                           "Auto/Inherit."};
    }
    auto next = p;
    next.queue_target_ms = values[0];
    next.destination.render_ahead_ms = values[1];
    next.common.period_ms = values[2];
    next.common.buffer_ms =
        values[3] ? std::optional<int>(static_cast<int>(*values[3])) : std::nullopt;
    next.source.period_ms = values[4];
    next.destination.period_ms = values[5];
    next.source.buffer_ms =
        values[6] ? std::optional<int>(static_cast<int>(*values[6])) : std::nullopt;
    next.destination.buffer_ms =
        values[7] ? std::optional<int>(static_cast<int>(*values[7])) : std::nullopt;
    const auto valid = validate_preferences(next);
    if (valid.ok)
        p = std::move(next);
    return valid;
}
Result parse_timing_values(const std::array<std::string, 8>& fields, TimingValues& out) {
    TimingValues values{};
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto& text = fields[i];
        if (text.empty())
            continue;
        double value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            return {false, "Enter positive milliseconds or clear the field for Auto/Inherit."};
        values[i] = value;
    }
    Preferences candidate;
    const auto result = set_timing_values(candidate, values);
    if (result.ok)
        out = values;
    return result;
}
std::string encode_timing_clipboard(const TimingValues& values) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto& n : values)
        array.push_back(n ? nlohmann::json(*n) : nlohmann::json(nullptr));
    return nlohmann::json{
        {"format", "audiohole-timing-settings"}, {"schema_version", 1}, {"values", array}}
        .dump();
}
Result decode_timing_clipboard(std::string_view bytes, TimingValues& out) {
    if (bytes.size() > 4096)
        return {false, "Buffer clipboard data exceeds its size limit."};
    try {
        std::set<std::string> keys;
        const auto j = nlohmann::json::parse(
            bytes, [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& v) {
                if (depth > 3)
                    throw std::invalid_argument("nested clipboard data");
                if (event == nlohmann::json::parse_event_t::key &&
                    !keys.insert(v.get<std::string>()).second)
                    throw std::invalid_argument("duplicate clipboard key");
                return true;
            });
        if (j.size() != 3 || j.at("format") != "audiohole-timing-settings" ||
            !j.at("schema_version").is_number_integer() || j.at("schema_version") != 1 ||
            !j.at("values").is_array() || j.at("values").size() != 8)
            return {false, "Clipboard does not contain supported AudioHole buffer values."};
        TimingValues values{};
        for (std::size_t i = 0; i < 8; ++i) {
            const auto& v = j.at("values").at(i);
            if (v.is_null())
                continue;
            if (!v.is_number())
                return {false, "Clipboard timing values must be numbers or null."};
            values[i] = v.get<double>();
        }
        Preferences candidate;
        const auto valid = set_timing_values(candidate, values);
        if (valid.ok)
            out = values;
        return valid;
    } catch (const std::exception&) {
        return {false, "Invalid AudioHole buffer clipboard data."};
    }
}
TimingValues timing_preset(unsigned index, const Endpoint* source, const Endpoint* destination,
                           const Preferences& p, std::optional<double> playback_capacity_ms) {
    if (index > 3)
        return {};
    const auto period = [&](const Endpoint* e,
                            const EndpointPreferences& overrides) -> std::optional<double> {
        if (!e || !e->inspection)
            return {};
        const auto effective = inherit_preferences(p.common, overrides);
        if (effective.mode.value_or("Shared") != "Shared")
            return {};
        const auto& info = *e->inspection;
        if ((effective.sample_rate_hz && effective.sample_rate_hz != info.period_sample_rate_hz) ||
            (effective.channel_count && effective.channel_count != info.period_channels) ||
            (effective.sample_format && *effective.sample_format != info.period_sample_format))
            return {};
        return index < 2 ? info.minimum_period_ms : info.default_period_ms;
    };
    const auto in = period(source, p.source), output = period(destination, p.destination);
    std::optional<double> queue;
    if (in && output) {
        const double interval = std::max(*in, *output);
        queue = index == 0   ? 1.2 * interval
                : index == 1 ? 2 * interval + 1
                : index == 2 ? 3.5 * interval
                             : 6 * interval;
    }
    if (queue && source && source->inspection && source->inspection->period_sample_rate_hz) {
        const auto rate = *source->inspection->period_sample_rate_hz;
        if (rate > 0)
            queue = 1000.0 * std::ceil(*queue * rate / 1000) / rate;
    }
    std::optional<double> lead;
    if (output && playback_capacity_ms) {
        double target = index == 0   ? *output + 0.5
                        : index == 1 ? *output + 1
                        : index == 2 ? *output + 5
                                     : 2 * *output;
        if (destination && destination->inspection &&
            destination->inspection->period_sample_rate_hz) {
            const auto rate = *destination->inspection->period_sample_rate_hz;
            if (rate > 0)
                target = 1000.0 * std::round(target * rate / 1000) / rate;
        }
        if (target <= *playback_capacity_ms)
            lead = target;
    }
    // Allocation is known only after opening the exact route. Do not offer a
    // numeric playback lead without confirming that it fits that allocation.
    return {queue, lead, {}, {}, in, output, {}, {}};
}
} // namespace audiohole
