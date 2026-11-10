#pragma once
#include "model/controller.h"
#include <cstdint>
#include <optional>
#include <string_view>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace audiohole::gui {
// Private Windows shell shortcuts forward only these fixed actions to the
// existing GUI. They are not a public routing API or persisted startup commands.
enum class ShellAction : std::uint32_t { Open, Controls, Start, Stop, Close, Exit };
inline constexpr wchar_t shell_app_id[] = L"AudioHole.PreviewBuild";
inline constexpr ULONG_PTR shell_message_tag = 0x41484F4C;
inline constexpr UINT shell_controls_command = 0x1E00, shell_exit_command = 0x1E10;
std::optional<ShellAction> parse_shell_action(std::wstring_view);
std::optional<ShellAction> decode_shell_action(const COPYDATASTRUCT*);
HWND find_application_window();
// Bounded synchronous admission only. Actual work is queued on the owner GUI
// thread after the sender returns; no new backend/data root is initialized.
bool forward_shell_action(HWND, ShellAction);
// GUI STA only. Uses system COM shell APIs; failure leaves the native system-menu
// controls usable. Smoke mode never publishes persistent Jump List tasks.
HRESULT publish_shell_tasks(bool routing_available);
Result configure_windows_startup(bool enabled);
} // namespace audiohole::gui
