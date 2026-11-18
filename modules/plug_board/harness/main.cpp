#include "runner.hpp"
#include "support.hpp"

#include <iostream>
#include <map>

namespace {
using namespace audiohole::plug_board::harness;
using namespace audiohole::plug_board::windows_detail;

// The console frontend shares the GUI's runner. Ctrl+C only requests cooperative
// cancellation; the runner still restores touched fixture preferences.
std::stop_source console_stop;
BOOL WINAPI interrupt_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        console_stop.request_stop();
        return TRUE;
    }
    return FALSE;
}

void print_help() {
    std::cout
        << "AudioHole Plug Board command-line test harness\n"
           "For the graphical application, open AudioHole-PlugBoard.exe at the package root.\n"
           "  inspect --output NEW_DIRECTORY\n"
           "  capability --output NEW_DIRECTORY\n"
           "  watch-starts --output NEW_DIRECTORY\n"
           "  exercise-fixture --endpoint WINDOWS_RENDER_ID --output NEW_DIRECTORY\n"
           "  recover-fixture --recovery RECOVERY_JSON --output NEW_DIRECTORY\n\n"
           "Only the disposable fixture can receive temporary routing preferences.\n";
}

std::filesystem::path executable_directory() {
    std::wstring path(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) {
        throw std::runtime_error("Could not find the executable directory.");
    }
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2 || std::wstring(argv[1]) == L"--help") {
        print_help();
        return 0;
    }
    try {
        const std::map<std::wstring, ProbeAction> actions{
            {L"inspect", ProbeAction::Inspect},
            {L"capability", ProbeAction::Capability},
            {L"watch-starts", ProbeAction::ProcessStarts},
            {L"exercise-fixture", ProbeAction::RoutingTest},
            {L"recover-fixture", ProbeAction::Recovery}};
        const auto action = actions.find(argv[1]);
        if (action == actions.end() || (argc - 2) % 2 != 0) {
            throw std::runtime_error("Use --help for complete harness commands.");
        }
        std::map<std::wstring, std::wstring> arguments;
        for (int index = 2; index < argc; index += 2) {
            const std::wstring name(argv[index]);
            const bool allowed =
                name == L"--output" ||
                (name == L"--endpoint" && action->second == ProbeAction::RoutingTest) ||
                (name == L"--recovery" && action->second == ProbeAction::Recovery);
            if (!allowed || !arguments.emplace(name, argv[index + 1]).second) {
                throw std::runtime_error("Unknown, duplicate or mode-inappropriate argument.");
            }
        }
        if (!arguments.contains(L"--output")) {
            throw std::runtime_error("Provide --output with a new results directory.");
        }
        ProbeRequest request;
        request.action = action->second;
        request.output_directory = arguments.at(L"--output");
        request.executable_directory = executable_directory();
        if (arguments.contains(L"--endpoint")) {
            request.endpoint_id = utf8(arguments.at(L"--endpoint"));
        }
        if (arguments.contains(L"--recovery")) {
            request.recovery_file = arguments.at(L"--recovery");
        }
        SetConsoleCtrlHandler(interrupt_handler, TRUE);
        const auto result = run_probe(request, console_stop.get_token());
        SetConsoleCtrlHandler(interrupt_handler, FALSE);
        std::cout << result.message << '\n';
        return result.exit_code;
    } catch (const std::exception& error) {
        SetConsoleCtrlHandler(interrupt_handler, FALSE);
        std::cerr << "Plug Board probe: " << error.what() << '\n';
        return 2;
    }
}
