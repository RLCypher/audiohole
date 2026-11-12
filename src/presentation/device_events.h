#pragma once
#include "model/controller.h"
#include "presentation/messages.h"
#include <map>
namespace audiohole::presentation {
struct DeviceNotice {
    Message message;
    std::string text;
};
// Presentation-only transition observer, control thread. Initial inventory,
// metadata-only edits, deletions/clear and unchanged polling emit nothing.
// Reported absence differs from other unavailability; no physical event is
// inferred from missing registry records or an unknown identity.
class DeviceEvents {
  public:
    std::vector<DeviceNotice> observe(const std::vector<Endpoint>&);

  private:
    struct Seen {
        Availability availability;
        std::string state;
    };
    std::map<std::string, Seen> previous_;
    bool seeded_ = false;
};
} // namespace audiohole::presentation
