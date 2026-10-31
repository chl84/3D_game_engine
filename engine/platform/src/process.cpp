#include <engine/platform/process.hpp>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#endif
namespace engine::platform {
bool process_running(int pid) noexcept {
    if (pid <= 0) {
        return false;
    }
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
        return GetLastError() != ERROR_INVALID_PARAMETER;
    }
    DWORD exit_code = 0;
    const bool active = GetExitCodeProcess(process, &exit_code) == 0 || exit_code == STILL_ACTIVE;
    (void)CloseHandle(process);
    return active;
#else
    return ::kill(pid, 0) == 0 || errno != ESRCH;
#endif
}
} // namespace engine::platform
