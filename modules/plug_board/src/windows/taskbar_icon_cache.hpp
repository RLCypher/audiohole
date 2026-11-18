#pragma once

#include "audiohole/plug_board/module.hpp"

#include <map>

namespace audiohole::plug_board::windows_detail {

// Shell-STA-owned, disposable presentation cache. Keys include complete verified
// application identity and executable file metadata, never a PID or display name.
// Only taskbar-derived pixels are remembered; an executable/stock fallback must
// not replace them when a window closes. This is separate from portable intent.
class TaskbarIconCache final {
  public:
    explicit TaskbarIconCache(std::filesystem::path file);
    const ApplicationImage* find(const std::string& key);
    void remember(const std::string& key, const ApplicationImage&);
    bool dirty() const {
        return dirty_;
    }
    OperationResult flush();
    const OperationResult& load_result() const {
        return load_result_;
    }

  private:
    struct Entry {
        ApplicationImage image;
        std::uint64_t used;
    };
    std::filesystem::path file_;
    std::map<std::string, Entry> entries_;
    std::uint64_t sequence_ = 0;
    bool dirty_ = false;
    bool writable_ = true;
    OperationResult load_result_{true, "icon_cache_empty", {}};
    void trim();
};

} // namespace audiohole::plug_board::windows_detail
