#pragma once
#include "controls.hpp"
#include <filesystem>

namespace audiohole::plug_board::desktop {
// Developer smoke only: inspect actual dialog/selection pixels and safe modal
// defaults. It neither mocks painting nor changes Windows audio/system settings.
void verify_popup_appearance(wxWindow&, const Appearance&, const std::filesystem::path&,
                             std::vector<std::string>&);
void verify_confirmation_cancel(wxWindow&, const Appearance&, const std::function<void()>&,
                                std::vector<std::string>&);
} // namespace audiohole::plug_board::desktop
