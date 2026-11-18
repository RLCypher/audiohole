#pragma once

#include "audiohole/plug_board/observations.hpp"
#include <memory>
#include <string>

namespace audiohole::plug_board::windows_detail {

// Worker-local snapshot. icon owns a CopyIcon result; no borrowed window HICON
// or HWND crosses into model snapshots. signature contains runtime-only cache
// evidence, never durable identity or configuration.
struct WindowIconSnapshot {
    std::shared_ptr<void> icon;
    std::string signature;
    std::string resource;
    std::string app_user_model_id;
    std::string icon_kind;
    bool class_fallback = false;
};
WindowIconSnapshot observe_window_icon(const ApplicationObservation&, unsigned pixels);

} // namespace audiohole::plug_board::windows_detail
