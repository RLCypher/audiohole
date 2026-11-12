#include "presentation/device_events.h"
namespace audiohole::presentation {
std::vector<DeviceNotice> DeviceEvents::observe(const std::vector<Endpoint>& endpoints) {
    std::map<std::string, Seen> next;
    std::vector<DeviceNotice> notices;
    for (const auto& device : endpoints) {
        const auto state = device.inspection ? device.inspection->observed_state : std::string{};
        const auto found = previous_.find(device.id);
        if (seeded_) {
            const bool available = device.availability == Availability::Available;
            if (available &&
                (found == previous_.end() || found->second.availability != Availability::Available))
                notices.push_back({Message::DeviceConnected, device.display_name + " connected."});
            else if (found != previous_.end() && device.availability == Availability::Unavailable &&
                     (found->second.availability == Availability::Available ||
                      ((state == "not_present" || state == "unplugged") &&
                       found->second.state == "disabled"))) {
                const bool absent = state == "not_present" || state == "unplugged";
                notices.push_back(
                    {absent ? Message::DeviceDisconnected : Message::DeviceUnavailable,
                     device.display_name + (absent ? " disconnected." : " became unavailable.")});
            }
        }
        next[device.id] = {device.availability, state};
    }
    previous_ = std::move(next);
    seeded_ = !endpoints.empty();
    return notices;
}
} // namespace audiohole::presentation
