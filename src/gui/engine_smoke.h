#pragma once
#include "model/controller.h"
namespace audiohole::gui {
// Private unattended fixture only; never called by ordinary startup/Tutorial.
std::unique_ptr<Controller> make_engine_smoke_controller();
} // namespace audiohole::gui
