#pragma once
#include <memory>
#include <optional>
#include <string>
namespace audiohole {
struct DiagnosticReading {
    bool active = false, capture = false;
    std::optional<float> peak;
    std::string target, detail;
    bool failed = false;
};
// Control-side interface. start/cancel never wait for audio or driver cleanup.
// A second operation is rejected until the preceding worker has released its
// client. No recordings, file I/O or routing configuration belong to a session.
class DiagnosticService {
  public:
    virtual ~DiagnosticService() = default;
    virtual bool start(const std::string& logical_id, const std::string& local_endpoint,
                       bool capture) = 0;
    virtual void cancel() = 0;
    virtual DiagnosticReading reading() const = 0;
};
} // namespace audiohole
