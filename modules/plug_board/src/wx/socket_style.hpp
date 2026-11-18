#pragma once
#include "audiohole/plug_board/library.hpp"
#include "controls.hpp"

namespace audiohole::plug_board::desktop {
ItemColors socket_heading_colors(const SocketAppearance&, const Appearance&);
wxBitmap socket_icon(const std::string&, const wxColour&, const wxSize&);
} // namespace audiohole::plug_board::desktop
