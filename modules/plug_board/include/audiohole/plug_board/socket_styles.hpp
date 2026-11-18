#pragma once
#include <array>
#include <string_view>

namespace audiohole::plug_board {
// Stable, portable appearance identifiers. These describe socket presentation;
// no color or icon is part of application identity, endpoint binding or routing.
struct SocketColor {
    std::string_view id, name;
    unsigned rgb;
};
inline constexpr std::array<SocketColor, 16> socket_colors{{{"red", "Red", 0xF44336},
                                                            {"rose", "Rose", 0xE91E63},
                                                            {"purple", "Purple", 0x9C27B0},
                                                            {"violet", "Violet", 0x673AB7},
                                                            {"indigo", "Indigo", 0x3F51B5},
                                                            {"blue", "Blue", 0x2196F3},
                                                            {"sky", "Sky", 0x03A9F4},
                                                            {"cyan", "Cyan", 0x00BCD4},
                                                            {"teal", "Teal", 0x009688},
                                                            {"green", "Green", 0x4CAF50},
                                                            {"lime", "Lime", 0xCDDC39},
                                                            {"yellow", "Yellow", 0xFFEB3B},
                                                            {"amber", "Amber", 0xFFC107},
                                                            {"orange", "Orange", 0xFF9800},
                                                            {"brown", "Brown", 0x795548},
                                                            {"slate", "Slate", 0x607D8B}}};
struct SocketIcon {
    std::string_view id, name;
};
inline constexpr std::array<SocketIcon, 18> socket_icons{{{"sports_esports", "Game"},
                                                          {"forum", "Chat"},
                                                          {"music_note", "Music"},
                                                          {"movie", "Media"},
                                                          {"language", "Browser"},
                                                          {"headphones", "Headphones"},
                                                          {"mic", "Microphone"},
                                                          {"volume_up", "Speaker"},
                                                          {"view_in_ar", "VR"},
                                                          {"live_tv", "TV"},
                                                          {"notifications", "Notifications"},
                                                          {"cable", "Cable"},
                                                          {"star", "Star"},
                                                          {"work", "Work"},
                                                          {"lock", "Lock"},
                                                          {"power_settings_new", "Power"},
                                                          {"desktop_windows", "PC"},
                                                          {"laptop_windows", "Laptop"}}};
} // namespace audiohole::plug_board
