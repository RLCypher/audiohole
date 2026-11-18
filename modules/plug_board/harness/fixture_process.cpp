#include "fixture_process.hpp"
#include "support.hpp"

#include <vector>

namespace audiohole::plug_board::harness {
using namespace windows_detail;
namespace {

class AttributeList {
  public:
    AttributeList(HANDLE input, HANDLE output) {
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        if (bytes == 0) {
            throw std::runtime_error("Could not size the fixture handle allowlist.");
        }
        storage_.resize(bytes);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        if (!InitializeProcThreadAttributeList(list_, 1, 0, &bytes)) {
            list_ = nullptr;
            throw std::runtime_error("Could not initialize the fixture handle allowlist.");
        }
        HANDLE handles[]{input, output};
        if (!UpdateProcThreadAttribute(list_, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                       sizeof(handles), nullptr, nullptr)) {
            DeleteProcThreadAttributeList(list_);
            list_ = nullptr;
            throw std::runtime_error("Could not restrict fixture handle inheritance.");
        }
    }
    ~AttributeList() {
        if (list_) {
            DeleteProcThreadAttributeList(list_);
        }
    }
    LPPROC_THREAD_ATTRIBUTE_LIST get() const {
        return list_;
    }

  private:
    std::vector<std::byte> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};

} // namespace

struct FixtureProcess::Impl {
    Handle process, input, output;
    ProcessToken token;
    bool stopped = false;

    ~Impl() {
        // Closing the private input pipe requests cooperative fixture shutdown
        // even when constructor validation or a later observation throws.
        input.reset();
        if (process && !stopped) {
            WaitForSingleObject(process.get(), INFINITE);
        }
    }

    void expect_reply(const std::string& expected) {
        std::string reply;
        const auto deadline = GetTickCount64() + 10000;
        while (GetTickCount64() < deadline && reply.size() < 1024) {
            DWORD available = 0;
            if (!PeekNamedPipe(output.get(), nullptr, 0, nullptr, &available, nullptr)) {
                throw std::runtime_error("Fixture output pipe closed before acknowledgement.");
            }
            if (available) {
                char character = 0;
                DWORD read = 0;
                if (!ReadFile(output.get(), &character, 1, &read, nullptr) || read != 1) {
                    throw std::runtime_error("Could not read fixture acknowledgement.");
                }
                if (character == '\n') {
                    if (reply != expected) {
                        throw std::runtime_error("Fixture replied: " + reply);
                    }
                    return;
                }
                if (character != '\r') {
                    reply.push_back(character);
                }
            } else if (WaitForSingleObject(process.get(), 10) == WAIT_OBJECT_0) {
                throw std::runtime_error("Fixture exited before acknowledgement.");
            }
        }
        throw std::runtime_error("Fixture acknowledgement was not received within ten seconds.");
    }
};

FixtureProcess::FixtureProcess(const std::filesystem::path& executable, bool metadata_only)
    : impl_(std::make_unique<Impl>()) {
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE input_read = nullptr, input_write = nullptr;
    if (!CreatePipe(&input_read, &input_write, &security, 0)) {
        throw std::runtime_error("Could not create private fixture input pipe.");
    }
    Handle child_input(input_read);
    impl_->input.reset(input_write);
    HANDLE output_read = nullptr, output_write = nullptr;
    if (!CreatePipe(&output_read, &output_write, &security, 0)) {
        throw std::runtime_error("Could not create private fixture output pipe.");
    }
    impl_->output.reset(output_read);
    Handle child_output(output_write);
    if (!SetHandleInformation(impl_->input.get(), HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(impl_->output.get(), HANDLE_FLAG_INHERIT, 0)) {
        throw std::runtime_error("Could not protect parent pipe ends from inheritance.");
    }
    AttributeList attributes(child_input.get(), child_output.get());
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = child_input.get();
    startup.StartupInfo.hStdOutput = child_output.get();
    startup.StartupInfo.hStdError = child_output.get();
    startup.lpAttributeList = attributes.get();
    PROCESS_INFORMATION process{};
    const auto absolute = std::filesystem::absolute(executable);
    // Pass an explicit application name to CreateProcessW: no shell, PATH search
    // or interpretation of a user-provided command string is involved.
    std::wstring command = L"\"" + absolute.wstring() + L"\"";
    if (metadata_only) {
        command += L" --metadata-only";
    }
    if (!CreateProcessW(absolute.c_str(), command.data(), nullptr, nullptr, TRUE,
                        EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup.StartupInfo, &process)) {
        throw std::runtime_error("Could not launch the disposable fixture executable.");
    }
    impl_->process.reset(process.hProcess);
    Handle initial_thread(process.hThread);
    child_input.reset();
    child_output.reset();
    impl_->expect_reply("ready");
    const auto metadata = inspect_process(process.dwProcessId);
    if (!metadata.result.ok) {
        throw std::runtime_error("Disposable fixture process identity could not be verified.");
    }
    impl_->token = metadata.token;
}
FixtureProcess::~FixtureProcess() = default;
const ProcessToken& FixtureProcess::token() const {
    return impl_->token;
}
void FixtureProcess::recreate_streams() {
    if (impl_->stopped) {
        throw std::runtime_error("Cannot recreate streams in a stopped fixture.");
    }
    const char command[] = "recreate\n";
    DWORD written = 0;
    if (!WriteFile(impl_->input.get(), command, sizeof(command) - 1, &written, nullptr) ||
        written != sizeof(command) - 1) {
        throw std::runtime_error("Could not request disposable fixture stream recreation.");
    }
    impl_->expect_reply("recreated");
}
void FixtureProcess::stop() {
    if (impl_->stopped) {
        return;
    }
    impl_->input.reset();
    if (WaitForSingleObject(impl_->process.get(), INFINITE) != WAIT_OBJECT_0) {
        throw std::runtime_error("Disposable fixture cleanup could not be confirmed.");
    }
    DWORD exit_code = 1;
    if (!GetExitCodeProcess(impl_->process.get(), &exit_code) || exit_code != 0) {
        throw std::runtime_error("Disposable fixture exited with a failure.");
    }
    impl_->stopped = true;
}

} // namespace audiohole::plug_board::harness
