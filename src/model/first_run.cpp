#include "model/first_run.h"
#include "model/persistence.h"
#include <fstream>
#include <nlohmann/json.hpp>
namespace audiohole {
Result license_acceptance_needed(const std::filesystem::path& receipt, bool& needed) {
    const auto checked = first_run_needed(receipt, needed);
    if (!checked.ok || needed)
        return checked;
    try {
        if (std::filesystem::file_size(receipt) > 4096)
            throw std::runtime_error("License receipt is too large.");
        std::ifstream input(receipt, std::ios::binary);
        const auto json = nlohmann::json::parse(input);
        if (json.at("format") != "audiohole-license-acceptance" || json.at("schema_version") != 1 ||
            json.at("accepted") != true)
            throw std::runtime_error("Invalid license acceptance receipt.");
        return {true, "License acceptance checked."};
    } catch (const std::exception& error) {
        return {false, std::string("Cannot read license acceptance: ") + error.what()};
    }
}
Result accept_license(const std::filesystem::path& receipt) {
    bool needed = true;
    const auto checked = license_acceptance_needed(receipt, needed);
    if (!checked.ok || !needed)
        return checked;
    return replace_document(receipt,
                            nlohmann::json{{"format", "audiohole-license-acceptance"},
                                           {"schema_version", 1},
                                           {"accepted", true}}
                                    .dump(2) +
                                "\n",
                            {});
}
namespace {
using Json = nlohmann::ordered_json;
std::string outcome_name(OnboardingOutcome outcome) {
    return outcome == OnboardingOutcome::Completed ? "completed"
           : outcome == OnboardingOutcome::Skipped ? "skipped"
                                                   : "none";
}
Json marker_document(OnboardingStatus status) {
    return {{"format", "audiohole-onboarding"},
            {"schema_version", 1},
            {"outcome", outcome_name(status.outcome)},
            {"greeting_shown", status.greeting_shown}};
}
std::string marker_bytes(const std::filesystem::path& marker) {
    if (std::filesystem::file_size(marker) > 4096)
        throw std::runtime_error("Onboarding marker is too large.");
    std::ifstream input(marker, std::ios::binary);
    if (!input)
        throw std::runtime_error("Onboarding marker is unreadable.");
    std::string bytes{std::istreambuf_iterator<char>(input), {}};
    if (input.bad() || bytes.size() > 4096)
        throw std::runtime_error("Onboarding marker read failed.");
    return bytes;
}
OnboardingStatus decode_status(const std::string& bytes) {
    const auto first = bytes.find_first_not_of(" \r\n\t");
    if (first == std::string::npos || bytes[first] != '{')
        return {}; // Older markers only recorded presence.
    const auto json = Json::parse(bytes);
    if (json.at("format") != "audiohole-onboarding" || json.at("schema_version") != 1 ||
        !json.at("greeting_shown").is_boolean())
        throw std::runtime_error("Invalid onboarding marker.");
    const auto outcome = json.at("outcome").get<std::string>();
    if (outcome != "none" && outcome != "completed" && outcome != "skipped")
        throw std::runtime_error("Invalid onboarding outcome.");
    return {outcome == "completed" ? OnboardingOutcome::Completed
            : outcome == "skipped" ? OnboardingOutcome::Skipped
                                   : OnboardingOutcome::None,
            json.at("greeting_shown").get<bool>()};
}
} // namespace
Result migrate_first_run_marker(const std::filesystem::path& legacy,
                                const std::filesystem::path& current) {
    const auto receipt = current.parent_path() / "onboarding-migration.json";
    try {
        bool current_missing = false, receipt_missing = false, legacy_missing = false;
        for (const auto& item :
             {std::pair{current, &current_missing}, std::pair{receipt, &receipt_missing}}) {
            const auto checked = first_run_needed(item.first, *item.second);
            if (!checked.ok)
                return checked;
        }
        if (!receipt_missing)
            return {true, "Legacy onboarding migration already checked."};
        if (current_missing) {
            const auto checked = first_run_needed(legacy, legacy_missing);
            if (!checked.ok)
                return checked;
            if (!legacy_missing) {
                const auto bytes = marker_bytes(legacy);
                (void)decode_status(bytes);
                const auto written = replace_document(current, bytes, {});
                if (!written.ok)
                    return written;
            }
        }
        // Written after the marker, so an interrupted copy remains retryable.
        // Keep the old installation read-only; it may be in Program Files.
        return replace_document(receipt,
                                Json{{"format", "audiohole-onboarding-migration"},
                                     {"schema_version", 1},
                                     {"legacy_checked", true}}
                                        .dump(2) +
                                    "\n",
                                {});
    } catch (const std::exception& error) {
        return {false, std::string("Cannot migrate onboarding state: ") + error.what()};
    }
}
Result first_run_needed(const std::filesystem::path& marker, bool& needed) {
    try {
        if (std::filesystem::is_symlink(marker) ||
            (std::filesystem::exists(marker) && !std::filesystem::is_regular_file(marker)))
            return {false, "The first-run marker must be a regular file."};
        needed = !std::filesystem::exists(marker);
        return {true, "First-run marker checked."};
    } catch (const std::exception&) {
        return {false, "Cannot read onboarding state in the AudioHole data folder."};
    }
}
Result mark_first_run(const std::filesystem::path& marker, OnboardingOutcome outcome) {
    bool needed = false;
    const auto checked = first_run_needed(marker, needed);
    if (!checked.ok || !needed)
        return checked;
    const auto written = replace_document(
        marker, marker_document({outcome, outcome == OnboardingOutcome::None}).dump(2) + "\n", {});
    return written.ok ? written
                      : Result{false, "Cannot save AudioHole.has-run in the data folder. Check "
                                      "folder permissions or external changes."};
}
Result read_onboarding_status(const std::filesystem::path& marker, OnboardingStatus& status) {
    bool needed = false;
    const auto checked = first_run_needed(marker, needed);
    if (!checked.ok)
        return checked;
    status = {};
    if (needed)
        return {true, "No onboarding receipt."};
    try {
        status = decode_status(marker_bytes(marker));
        return {true, "Onboarding receipt read."};
    } catch (const std::exception& error) {
        return {false, std::string("Cannot read onboarding receipt: ") + error.what()};
    }
}
Result mark_onboarding_greeting_shown(const std::filesystem::path& marker) {
    OnboardingStatus status;
    const auto read = read_onboarding_status(marker, status);
    if (!read.ok || status.greeting_shown)
        return read;
    try {
        const auto expected = marker_bytes(marker);
        status = decode_status(expected);
        status.greeting_shown = true;
        return replace_document(marker, marker_document(status).dump(2) + "\n", expected);
    } catch (const std::exception& error) {
        return {false, std::string("Cannot save onboarding greeting receipt: ") + error.what()};
    }
}
Result reset_first_run(const std::filesystem::path& marker) {
    bool needed = false;
    const auto checked = first_run_needed(marker, needed);
    if (!checked.ok || needed)
        return checked;
    try {
        if (!std::filesystem::remove(marker))
            return {false, "The first-run marker could not be removed."};
        return {true, "First-run marker removed; restart to replay the welcome/tutorial."};
    } catch (const std::exception&) {
        return {false,
                "Cannot remove onboarding state in the data folder. Check folder permissions."};
    }
}
} // namespace audiohole
