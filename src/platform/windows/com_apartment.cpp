#include "platform/windows/com_apartment.h"

namespace audiohole::windows {

ComApartment::ComApartment(ApartmentModel model) noexcept
    : result_(CoInitializeEx(nullptr, model == ApartmentModel::multi_threaded
                                         ? COINIT_MULTITHREADED
                                         : COINIT_APARTMENTTHREADED)) {}

ComApartment::~ComApartment() noexcept {
    if (initialized()) {
        CoUninitialize();
    }
}

}  // namespace audiohole::windows
