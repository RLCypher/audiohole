#include "audiohole/deep_scan.hpp"
#include "storage_internal.hpp"

#include <chrono>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <algorithm>
#include <vector>

namespace audiohole::scanner {
namespace {
struct Handle {
  HANDLE value = nullptr;
  ~Handle() { reset(); }
  void reset() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); value = nullptr; }
};
struct AttributeList {
  std::vector<unsigned char> storage;
  LPPROC_THREAD_ATTRIBUTE_LIST value = nullptr;
  ~AttributeList() { if (value) DeleteProcThreadAttributeList(value); }
};
class ProcessRunner final : public ProbeRunner {
 public:
  explicit ProcessRunner(fs::path executable) : executable_(std::move(executable)) {}
  Json execute(const ProbeJob& job, std::stop_token stop, const ProbeStageSink& stage) override {
    Json stages = Json::array();
    auto incomplete = [&](const std::string& reason, const std::string& cleanup) {
      return Json{{"classification", "inconclusive"}, {"cleanup", cleanup}, {"reason", reason},
        {"stages", stages}, {"verification_scope", "buffer_processing"}, {"signal_verification", "not_tested"}};
    };
    if (stop.stop_requested()) return incomplete("Canceled before worker launch", "completed");
    (void)parse_probe_job(probe_job_json(job));
    Handle child_job, input_read, input_write, output_read, output_write, cancel, process, thread;
    child_job.value = CreateJobObjectW(nullptr, nullptr);
    if (!child_job.value) throw std::runtime_error("Cannot create probe Job Object");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit = 256ULL * 1024 * 1024;
    if (!SetInformationJobObject(child_job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
      throw std::runtime_error("Cannot bound probe worker resources");
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if (!CreatePipe(&input_read.value, &input_write.value, &security, 65536) ||
        !CreatePipe(&output_read.value, &output_write.value, &security, 65536))
      throw std::runtime_error("Cannot create private probe pipes");
    if (!SetHandleInformation(input_write.value, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(output_read.value, HANDLE_FLAG_INHERIT, 0))
      throw std::runtime_error("Cannot restrict probe pipe inheritance");
    cancel.value = CreateEventW(&security, TRUE, FALSE, nullptr);
    if (!cancel.value) throw std::runtime_error("Cannot create worker cancellation event");
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    AttributeList attributes; attributes.storage.resize(size);
    attributes.value = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.storage.data());
    if (!InitializeProcThreadAttributeList(attributes.value, 1, 0, &size)) {
      attributes.value = nullptr; throw std::runtime_error("Cannot initialize worker handle list");
    }
    HANDLE inherited[]{input_read.value, output_write.value, cancel.value};
    if (!UpdateProcThreadAttribute(attributes.value, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
      inherited, sizeof(inherited), nullptr, nullptr)) throw std::runtime_error("Cannot set worker handle list");
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input_read.value;
    startup.StartupInfo.hStdOutput = startup.StartupInfo.hStdError = output_write.value;
    startup.lpAttributeList = attributes.value;
    PROCESS_INFORMATION info{};
    const auto path = fs::absolute(executable_).wstring();
    auto command = L"\"" + path + L"\" --probe-worker";
    if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, TRUE,
      EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr,
      &startup.StartupInfo, &info)) throw std::runtime_error("Cannot launch isolated probe worker");
    process.value = info.hProcess; thread.value = info.hThread;
    if (!AssignProcessToJobObject(child_job.value, process.value)) {
      TerminateProcess(process.value, 2);
      throw std::runtime_error("Cannot contain probe worker in Job Object");
    }
    Json request = probe_job_json(job);
    request["cancel_handle"] = reinterpret_cast<std::uintptr_t>(cancel.value);
    const auto bytes = request.dump() + '\n';
    if (bytes.size() > 16384) throw std::runtime_error("Probe request exceeds IPC size budget");
    const auto launched = std::chrono::steady_clock::now();
    if (ResumeThread(thread.value) == static_cast<DWORD>(-1)) throw std::runtime_error("Cannot resume probe worker");
    thread.reset(); input_read.reset(); output_write.reset();
    DWORD written = 0;
    if (!WriteFile(input_write.value, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) || written != bytes.size())
      throw std::runtime_error("Cannot send private probe request");
    input_write.reset();
    std::string pending;
    std::size_t total_bytes = 0;
    Json result;
    bool received_result = false, exited = false;
    std::string problem;
    auto deadline = launched + std::chrono::milliseconds(job.timeout_ms);
    bool cancel_sent = false;
    while (true) {
      if (stop.stop_requested() && !cancel_sent) {
        SetEvent(cancel.value); cancel_sent = true;
        deadline = std::min(deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(500));
      }
      DWORD available = 0;
      if (PeekNamedPipe(output_read.value, nullptr, 0, nullptr, &available, nullptr) && available) {
        char chunk[4096]; DWORD count = 0;
        if (!ReadFile(output_read.value, chunk, std::min<DWORD>(available, sizeof(chunk)), &count, nullptr)) {
          problem = "Probe IPC read failed"; break;
        }
        total_bytes += count; pending.append(chunk, count);
        if (total_bytes > 1024 * 1024 || pending.size() > 65536) { problem = "Probe IPC exceeded size budget"; break; }
        try {
          std::size_t newline = 0;
          while ((newline = pending.find('\n')) != std::string::npos) {
            // Stage consumers may perform journal I/O. Recheck cancellation and
            // deadlines between messages instead of starving them while output
            // remains available. A completed child may still have queued evidence.
            if (stop.stop_requested() && !cancel_sent) {
              SetEvent(cancel.value); cancel_sent = true;
              deadline = std::min(deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(500));
            }
            if (std::chrono::steady_clock::now() >= deadline && WaitForSingleObject(process.value, 0) != WAIT_OBJECT_0)
              throw std::runtime_error(cancel_sent ? "Probe canceled" : "Probe attempt timed out");
            auto message = detail::parse_json(pending.substr(0, newline)); pending.erase(0, newline + 1);
            if (message.value("attempt_id", "") != job.attempt_id) throw std::runtime_error("Probe attempt ID mismatch");
            if (message.value("type", "") == "stage" && !received_result) {
              const auto& row = message.at("stage");
              if (stages.size() >= 128 || !row.is_object() || !row.contains("api") || !row["api"].is_string() ||
                  (row.value("phase", "") != "begin" && row.value("phase", "") != "end"))
                throw std::runtime_error("Invalid stage stream");
              stages.push_back(message["stage"]);
              stage(message["stage"]);
            } else if (message.value("type", "") == "result" && !received_result) {
              result = message.at("result"); received_result = true;
              if (!result.is_object()) throw std::runtime_error("Invalid worker result");
            } else throw std::runtime_error("Invalid worker message sequence");
          }
        } catch (const std::exception& e) { problem = e.what(); break; }
        continue; // Drain output before accepting worker exit.
      }
      if (WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0) { exited = true; break; }
      if (std::chrono::steady_clock::now() >= deadline) { problem = cancel_sent ? "Probe canceled" : "Probe attempt timed out"; break; }
      WaitForSingleObject(process.value, 10);
    }
    if (!exited) {
      SetEvent(cancel.value);
      if (WaitForSingleObject(process.value, 500) != WAIT_OBJECT_0) TerminateJobObject(child_job.value, 2);
      exited = WaitForSingleObject(process.value, 1000) == WAIT_OBJECT_0;
      auto outcome = incomplete(problem.empty() ? "Worker interrupted" : problem, "unknown");
      outcome["process_exit_observed"] = exited;
      return outcome;
    }
    DWORD exit_code = 2; GetExitCodeProcess(process.value, &exit_code);
    if (!problem.empty() || !received_result || !pending.empty() || exit_code != 0) {
      auto outcome = incomplete(problem.empty() ? "Worker exited without a complete valid result" : problem, "unknown");
      outcome["worker_exit_code"] = exit_code;
      outcome["process_exit_observed"] = true;
      return outcome;
    }
    try {
      const auto classification = result.value("classification", "");
      if (classification != "verified" && classification != "accepted" && classification != "failed" && classification != "inconclusive")
        return incomplete("Invalid worker classification", "unknown");
      if (classification == "verified" && (!result.contains("runtime_duration_ms") || !result["runtime_duration_ms"].is_number_integer() ||
          result["runtime_duration_ms"] < job.duration_ms || !result.contains("serviced_frames") ||
          !result["serviced_frames"].is_number_integer() || result["serviced_frames"] <= 0 ||
          !result.contains("audio_events") || !result["audio_events"].is_number_integer() || result["audio_events"] < 3 ||
          result.value("cleanup", "") != "completed" || result.value("verification_scope", "") != "buffer_processing"))
        return incomplete("Worker verification lacks required runtime evidence", "unknown");
    } catch (const std::exception&) { return incomplete("Malformed worker result fields", "unknown"); }
    result["stages"] = std::move(stages);
    return result;
  }
 private:
  fs::path executable_;
};
}
std::unique_ptr<ProbeRunner> make_process_probe_runner(const fs::path& executable) {
  return std::make_unique<ProcessRunner>(executable);
}
}
#else
namespace audiohole::scanner {
std::unique_ptr<ProbeRunner> make_process_probe_runner(const fs::path&) {
  throw std::runtime_error("Active Deep Scan workers require Windows");
}
}
#endif
