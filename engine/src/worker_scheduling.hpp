#pragma once
#include "audiohole/engine/backend.hpp"
#include <atomic>
#include <windows.h>

// AVRT declarations require Win32 types, including in the standalone miniaudio TU.
#include <avrt.h>

namespace audiohole::engine::detail {
// start/stop belong to the SAME audio worker, outside sample processing. The
// lifecycle observer only reads atomic status; it never borrows the MMCSS handle.
// Each adapter must stop registration before its worker exits, including errors.
class WorkerScheduling {
  public:
    explicit WorkerScheduling(WorkerSettings settings = {}) : settings_(settings) {}
    void start() noexcept {
        if (handle_)
            stop();
        DWORD index = 0;
        handle_ = AvSetMmThreadCharacteristicsW(
            settings_.scheduling == SchedulingTask::Audio ? L"Audio" : L"Pro Audio", &index);
        const bool priority =
            handle_ && AvSetMmThreadPriority(handle_, settings_.priority == AudioPriority::High
                                                          ? AVRT_PRIORITY_HIGH
                                                          : AVRT_PRIORITY_NORMAL);
        if (!priority)
            error_.store(GetLastError(), std::memory_order_relaxed);
        state_.store(priority ? 1 : handle_ ? 3 : 2, std::memory_order_release);
    }
    void stop() noexcept {
        if (handle_) {
            AvRevertMmThreadCharacteristics(handle_);
            handle_ = nullptr;
            state_.store(4, std::memory_order_release);
        }
    }
    std::string describe() const {
        const auto state = state_.load(std::memory_order_acquire);
        const std::string policy =
            std::string("MMCSS ") +
            (settings_.scheduling == SchedulingTask::Audio ? "Audio" : "Pro Audio") + " / " +
            (settings_.priority == AudioPriority::High ? "High" : "Normal");
        if (state == 1)
            return policy + " applied";
        if (state == 4)
            return policy + " inactive";
        if (state == 0)
            return policy + " pending";
        return policy +
               (state == 3 ? " task applied; priority unavailable (Windows "
                           : " registration unavailable (Windows ") +
               std::to_string(error_.load(std::memory_order_relaxed)) + ")";
    }

  private:
    WorkerSettings settings_;
    HANDLE handle_ = nullptr; // Audio-worker owned; never read by the observer.
    std::atomic<int> state_{0};
    std::atomic<DWORD> error_{0};
};
} // namespace audiohole::engine::detail
