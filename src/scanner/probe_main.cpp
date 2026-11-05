#include "audiohole/deep_scan.hpp"
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#endif

// Private stdin/stdout worker protocol only; no device discovery or broad scans.
int main(int argc, char** argv) {
    if (argc != 2 || std::string_view(argv[1]) != "--probe-worker")
        return 2;
        // A driver fault must not leave an interactive crash dialog behind. The
        // parent owns timeouts and Job Object cleanup, and reports inconclusive evidence.
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    return audiohole::scanner::run_probe_worker();
}
