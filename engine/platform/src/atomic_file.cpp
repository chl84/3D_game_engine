#include <engine/platform/atomic_file.hpp>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace engine::platform {
namespace {

[[nodiscard]] core::Error io_error(std::string message, const std::filesystem::path& path,
                                   std::string hint = {}) {
    return core::make_error(core::ErrorCode::Io, std::move(message), std::move(hint),
                            {{"path", path.string()}});
}

[[nodiscard]] std::filesystem::path temporary_path(const std::filesystem::path& path) {
    static std::atomic<std::uint64_t> counter{0};
#ifdef _WIN32
    const int pid = _getpid();
#else
    const int pid = ::getpid();
#endif
    std::filesystem::path temporary = path;
    temporary += ".tmp." + std::to_string(pid) + "." + std::to_string(counter.fetch_add(1));
    return temporary;
}

#ifndef _WIN32
void sync_directory(const std::filesystem::path& path) {
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (descriptor >= 0) {
        (void)::fsync(descriptor);
        (void)::close(descriptor);
    }
}
#endif

} // namespace

core::Result<void> atomic_write_file(const std::filesystem::path& path, std::string_view content) {
    const std::filesystem::path temporary = temporary_path(path);

#ifndef _WIN32
    int flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    const int descriptor = ::open(temporary.c_str(), flags, 0600);
    if (descriptor < 0) {
        return core::make_failure<void>(
            io_error("could not create temporary file", temporary, std::strerror(errno)));
    }

    std::size_t written = 0;
    while (written < content.size()) {
        const ssize_t result =
            ::write(descriptor, content.data() + written, content.size() - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            const std::string message = std::strerror(errno);
            (void)::close(descriptor);
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return core::make_failure<void>(
                io_error("could not write temporary file", temporary, message));
        }
        written += static_cast<std::size_t>(result);
    }

    if (::fsync(descriptor) != 0) {
        const std::string message = std::strerror(errno);
        (void)::close(descriptor);
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return core::make_failure<void>(
            io_error("could not sync temporary file", temporary, message));
    }
    (void)::close(descriptor);

    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return core::make_failure<void>(
            io_error("could not replace target file", path, error.message()));
    }
    if (path.has_parent_path()) {
        sync_directory(path.parent_path());
    }
#else
    HANDLE handle = CreateFileW(temporary.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return core::make_failure<void>(io_error("could not create temporary file", temporary));
    }

    std::size_t written = 0;
    while (written < content.size()) {
        const DWORD chunk =
            static_cast<DWORD>(std::min<std::size_t>(content.size() - written, 1U << 20U));
        DWORD actual = 0;
        if (!WriteFile(handle, content.data() + written, chunk, &actual, nullptr)) {
            (void)CloseHandle(handle);
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return core::make_failure<void>(io_error("could not write temporary file", temporary));
        }
        written += static_cast<std::size_t>(actual);
    }
    if (!FlushFileBuffers(handle)) {
        (void)CloseHandle(handle);
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return core::make_failure<void>(io_error("could not sync temporary file", temporary));
    }
    (void)CloseHandle(handle);

    if (!MoveFileExW(temporary.wstring().c_str(), path.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return core::make_failure<void>(io_error("could not replace target file", path));
    }
#endif
    return {};
}

} // namespace engine::platform
