#pragma once
#include "audiohole/scanner.hpp"
#ifdef _WIN32
#include <audioclient.h>
namespace audiohole::scanner::detail {
Json describe_wave_format(const WAVEFORMATEX* format, std::size_t size);
}
#endif
