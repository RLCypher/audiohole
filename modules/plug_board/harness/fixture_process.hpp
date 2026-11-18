#pragma once

#include "audiohole/plug_board/windows.hpp"
#include <memory>

namespace audiohole::plug_board::harness {

// Owns only the deliberately launched fixture, communicating through anonymous
// inherited pipes. It cannot attach to or send commands to another application.
// Child cleanup is cooperative: close stdin, stop streams, join the fixture, then
// release handles. It never terminates a user process or promises a driver timeout.
class FixtureProcess {
  public:
    FixtureProcess(const std::filesystem::path& executable, bool metadata_only = false);
    ~FixtureProcess();
    FixtureProcess(const FixtureProcess&) = delete;
    FixtureProcess& operator=(const FixtureProcess&) = delete;
    const ProcessToken& token() const;
    void recreate_streams();
    void stop();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace audiohole::plug_board::harness
