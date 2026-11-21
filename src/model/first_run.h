#pragma once
#include "model/controller.h"
#include <filesystem>
namespace audiohole {
enum class OnboardingOutcome { None, Completed, Skipped };
struct OnboardingStatus {
    OnboardingOutcome outcome = OnboardingOutcome::None;
    bool greeting_shown = true;
};
// Control-thread filesystem operations. Marker lives in the working data root.
// Errors never silently redirect it to another directory.
// Copies legacy status/receipt exactly once without requiring writes to Program
// Files. A persistent migration receipt prevents Replay from importing it again.
Result migrate_first_run_marker(const std::filesystem::path& legacy,
                                const std::filesystem::path& current);
Result first_run_needed(const std::filesystem::path& marker, bool& needed);
Result mark_first_run(const std::filesystem::path& marker,
                      OnboardingOutcome = OnboardingOutcome::None);
Result reset_first_run(const std::filesystem::path& marker);
// Legacy presence-only markers remain valid and do not create a surprise greeting.
// Outcome/receipt writes use the same marker and atomic external-edit protection.
Result read_onboarding_status(const std::filesystem::path&, OnboardingStatus&);
Result mark_onboarding_greeting_shown(const std::filesystem::path&);
// Independent of tutorial completion/replay. Acceptance is one-time per user.
Result license_acceptance_needed(const std::filesystem::path&, bool& needed);
Result accept_license(const std::filesystem::path&);
} // namespace audiohole
