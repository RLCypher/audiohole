#pragma once
#include "model/update_policy.h"
#include <functional>
#include <memory>
#include <span>
#include <stop_token>

namespace audiohole::windows {
// Network calls and file/hash work run on the service worker, never an audio or
// GUI callback. A test transport supplies bytes without accessing the network.
class UpdateTransport {
  public:
    using Sink = std::function<void(std::span<const std::byte>)>;
    virtual ~UpdateTransport() = default;
    virtual void fetch(const std::string& url, std::uint64_t maximum_bytes, std::stop_token,
                       const Sink&) = 0;
    virtual void cancel() = 0;
};
class DownloadedInstaller {
  public:
    ~DownloadedInstaller();
    const std::filesystem::path& path() const;
    // Called only after ShellExecute successfully starts this verified package.
    void retain_for_installer();

  private:
    friend class UpdateService;
    struct Impl;
    explicit DownloadedInstaller(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
struct UpdateResult {
    std::optional<updates::Release> release;
    std::shared_ptr<DownloadedInstaller> installer;
    std::string error;
    bool cancelled = false;
};
class UpdateService {
  public:
    explicit UpdateService(std::filesystem::path cache,
                           std::unique_ptr<UpdateTransport> transport = {});
    ~UpdateService();
    bool check();
    bool download(const updates::Release&);
    bool busy() const;
    void cancel();
    std::optional<UpdateResult> take_result();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace audiohole::windows
