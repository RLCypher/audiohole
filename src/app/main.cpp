#include "platform/windows/com_apartment.h"

#include <cstdint>
#include <iomanip>
#include <iostream>

int main() {
    const audiohole::windows::ComApartment apartment;
    if (!apartment.initialized()) {
        std::cerr << "COM initialization failed: HRESULT 0x" << std::hex
                  << std::uppercase << std::setfill('0') << std::setw(8)
                  << static_cast<std::uint32_t>(apartment.result()) << '\n';
        return 1;
    }

    std::cout << "AudioHole development foundation: COM initialized. "
                 "Audio routing is not implemented.\n";
    return 0;
}
