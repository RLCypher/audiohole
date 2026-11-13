#pragma once
#include "model/controller.h"
#include <filesystem>
namespace audiohole {
enum class OnboardingOutcome { None, Completed, Skipped };
struct OnboardingStatus {
    OnboardingOutcome outcome = OnboardingOutcome::None;
    bool greeting_shown = true;
};
// Control-thread filesystem operations. Marker lives in the application bin/ folder, not
// in a registry key. Errors never silently redirect it to another directory.
Result first_run_needed(const std::filesystem::path& marker, bool& needed);
Result mark_first_run(const std::filesystem::path& marker,
                      OnboardingOutcome = OnboardingOutcome::None);
Result reset_first_run(const std::filesystem::path& marker);
// Legacy presence-only markers remain valid and do not create a surprise greeting.
// Outcome/receipt writes use the same marker and atomic external-edit protection.
Result read_onboarding_status(const std::filesystem::path&, OnboardingStatus&);
Result mark_onboarding_greeting_shown(const std::filesystem::path&);
} // namespace audiohole
