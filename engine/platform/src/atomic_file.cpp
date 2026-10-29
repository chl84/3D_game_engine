#include <engine/platform/atomic_file.hpp>

#include <fstream>
#include <system_error>
#include <utility>

#ifdef _WIN32
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

#ifndef _WIN32
void sync_file(const std::filesystem::path& path) {
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor >= 0) {
        (void)::fsync(descriptor);
        (void)::close(descriptor);
    }
}

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
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            return core::make_failure<void>(
                io_error("could not open temporary file for writing", temporary));
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.flush();
        if (!output) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return core::make_failure<void>(io_error("could not write temporary file", temporary));
        }
    }

#ifndef _WIN32
    sync_file(temporary);
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
