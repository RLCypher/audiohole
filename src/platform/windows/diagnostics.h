#pragma once
#include "model/diagnostic_service.h"
namespace audiohole::windows {
std::unique_ptr<DiagnosticService> make_diagnostic_service();
} // namespace audiohole::windows
