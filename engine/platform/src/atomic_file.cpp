#include <engine/platform/atomic_file.hpp>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>

#include <aclapi.h>
#include <process.h>
#include <sddl.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
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

namespace {
#ifdef _WIN32
struct PrivateSecurity {
    PSECURITY_DESCRIPTOR descriptor{nullptr};
    ~PrivateSecurity() {
        if (descriptor != nullptr) {
            (void)LocalFree(descriptor);
        }
    }
    bool verify(HANDLE handle) const {
        PACL actual = nullptr;
        PSID actual_owner = nullptr;
        PSECURITY_DESCRIPTOR info = nullptr;
        if (GetSecurityInfo(handle, SE_FILE_OBJECT,
                            DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, &actual_owner,
                            nullptr, &actual, nullptr, &info) != ERROR_SUCCESS) {
            (void)LocalFree(info);
            return false;
        }
        SECURITY_DESCRIPTOR_CONTROL control{};
        DWORD revision = 0;
        BOOL present = FALSE;
        BOOL defaulted = FALSE;
        PACL expected = nullptr;
        PSID expected_owner = nullptr;
        void* actual_entry = nullptr;
        void* expected_entry = nullptr;
        bool valid = actual != nullptr && actual->AceCount == 1 &&
                     GetSecurityDescriptorControl(info, &control, &revision) != 0 &&
                     (control & SE_DACL_PROTECTED) != 0 &&
                     GetSecurityDescriptorOwner(descriptor, &expected_owner, &defaulted) != 0 &&
                     actual_owner != nullptr && expected_owner != nullptr &&
                     EqualSid(actual_owner, expected_owner) != 0 &&
                     GetSecurityDescriptorDacl(descriptor, &present, &expected, &defaulted) != 0 &&
                     present != FALSE && expected != nullptr &&
                     GetAce(actual, 0, &actual_entry) != 0 &&
                     GetAce(expected, 0, &expected_entry) != 0;
        if (valid) {
            auto* a = static_cast<ACCESS_ALLOWED_ACE*>(actual_entry);
            auto* e = static_cast<ACCESS_ALLOWED_ACE*>(expected_entry);
            valid = a->Header.AceType == ACCESS_ALLOWED_ACE_TYPE &&
                    (a->Header.AceFlags & INHERITED_ACE) == 0 && a->Mask == e->Mask &&
                    EqualSid(&a->SidStart, &e->SidStart) != 0;
        }
        (void)LocalFree(info);
        return valid;
    }
    bool initialize() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            return false;
        }
        DWORD size = 0;
        (void)GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        std::vector<unsigned char> buffer(size);
        const bool got_user =
            GetTokenInformation(token, TokenUser, buffer.data(), size, &size) != 0;
        (void)CloseHandle(token);
        if (!got_user) {
            return false;
        }
        const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
        LPWSTR sid = nullptr;
        if (!ConvertSidToStringSidW(user->User.Sid, &sid)) {
            return false;
        }
        const std::wstring current_user(sid);
        const std::wstring sddl = L"O:" + current_user + L"D:P(A;;FA;;;" + current_user + L")";
        (void)LocalFree(sid);
        return ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
                                                                    &descriptor, nullptr) != 0;
    }
};
#endif
core::Result<void> write_file(const std::filesystem::path& path, std::string_view content,
                              bool owner_only) {
    const std::filesystem::path temporary = temporary_path(path);

#ifndef _WIN32
    unsigned flags = static_cast<unsigned>(O_WRONLY) | static_cast<unsigned>(O_CREAT) |
                     static_cast<unsigned>(O_EXCL);
#ifdef O_NOFOLLOW
    flags |= static_cast<unsigned>(O_NOFOLLOW);
#endif
    const int descriptor =
        ::open(temporary.c_str(), static_cast<int>(flags), owner_only ? 0600 : 0666);
    if (descriptor < 0) {
        return core::make_failure<void>(
            io_error("could not create temporary file", temporary, std::strerror(errno)));
    }

    struct stat target_status = {};
    struct stat private_status = {};
    if (owner_only &&
        (::fchmod(descriptor, 0600) != 0 || ::fstat(descriptor, &private_status) != 0 ||
         (private_status.st_mode & 0777U) != 0600 || private_status.st_uid != ::geteuid())) {
        (void)::close(descriptor);
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return core::make_failure<void>(io_error("could not restrict private file", temporary));
    }
    if (!owner_only && ::stat(path.c_str(), &target_status) == 0) {
        (void)::fchmod(descriptor, target_status.st_mode & 07777U);
    }

    std::size_t written = 0;
    while (written < content.size()) {
        const ssize_t result =
            ::write(descriptor, content.data() + written, content.size() - written);
        if (result <= 0) {
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
    PrivateSecurity security;
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    if (owner_only) {
        if (!security.initialize()) {
            return core::make_failure<void>(io_error("could not restrict private file", path));
        }
        attributes.lpSecurityDescriptor = security.descriptor;
        attributes.bInheritHandle = FALSE;
    }
    HANDLE handle = CreateFileW(
        temporary.wstring().c_str(), owner_only ? GENERIC_WRITE | READ_CONTROL : GENERIC_WRITE, 0,
        owner_only ? &attributes : nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return core::make_failure<void>(io_error("could not create temporary file", temporary));
    }

    if (owner_only && !security.verify(handle)) {
        (void)CloseHandle(handle);
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return core::make_failure<void>(
            io_error("could not verify private file permissions", temporary));
    }
    std::size_t written = 0;
    while (written < content.size()) {
        const DWORD chunk =
            static_cast<DWORD>(std::min<std::size_t>(content.size() - written, 1U << 20U));
        DWORD actual = 0;
        if (!WriteFile(handle, content.data() + written, chunk, &actual, nullptr) || actual == 0) {
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

} // namespace
core::Result<void> atomic_write_file(const std::filesystem::path& path, std::string_view content) {
    return write_file(path, content, false);
}
core::Result<void> atomic_write_private_file(const std::filesystem::path& path,
                                             std::string_view content) {
    return write_file(path, content, true);
}
core::Result<void> append_private_file_synced(const std::filesystem::path& path,
                                              std::string_view content) {
#ifndef _WIN32
    unsigned flags = static_cast<unsigned>(O_WRONLY) | static_cast<unsigned>(O_APPEND) |
                     static_cast<unsigned>(O_NONBLOCK) | static_cast<unsigned>(O_CLOEXEC);
#ifdef O_NOFOLLOW
    flags |= static_cast<unsigned>(O_NOFOLLOW);
#endif
    const int descriptor = ::open(path.c_str(), static_cast<int>(flags));
    if (descriptor < 0) {
        return core::make_failure<void>(io_error("could not open private journal", path));
    }
    struct CloseFile {
        int descriptor;
        ~CloseFile() {
            (void)::close(descriptor);
        }
    };
    const CloseFile guard{descriptor};
    const auto appended = [&]() -> core::Result<void> {
        struct stat status = {};
        if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) || status.st_nlink != 1 ||
            (status.st_mode & 0777U) != 0600 || status.st_uid != ::geteuid()) {
            return core::make_failure<void>(io_error("invalid private journal", path));
        }
        std::size_t written = 0;
        while (written < content.size()) {
            const auto count =
                ::write(descriptor, content.data() + written, content.size() - written);
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                return core::make_failure<void>(io_error("could not append private journal", path));
            }
            written += static_cast<std::size_t>(count);
        }
        if (::fsync(descriptor) != 0) {
            return core::make_failure<void>(io_error("could not sync private journal", path));
        }
        return {};
    }();
#else
    HANDLE handle =
        CreateFileW(path.wstring().c_str(), GENERIC_WRITE | READ_CONTROL, FILE_SHARE_READ, nullptr,
                    OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return core::make_failure<void>(io_error("could not open private journal", path));
    }
    struct CloseFile {
        HANDLE handle;
        ~CloseFile() {
            (void)CloseHandle(handle);
        }
    };
    const CloseFile guard{handle};
    const auto appended = [&]() -> core::Result<void> {
        PrivateSecurity security;
        BY_HANDLE_FILE_INFORMATION info{};
        LARGE_INTEGER end{};
        if (!security.initialize() || !security.verify(handle) ||
            !GetFileInformationByHandle(handle, &info) || info.nNumberOfLinks != 1 ||
            (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) !=
                0 ||
            GetFileType(handle) != FILE_TYPE_DISK ||
            !SetFilePointerEx(handle, end, nullptr, FILE_END)) {
            return core::make_failure<void>(io_error("invalid private journal", path));
        }
        std::size_t written = 0;
        while (written < content.size()) {
            const auto chunk =
                static_cast<DWORD>(std::min<std::size_t>(content.size() - written, 1U << 20U));
            DWORD count = 0;
            if (!WriteFile(handle, content.data() + written, chunk, &count, nullptr) ||
                count == 0) {
                return core::make_failure<void>(io_error("could not append private journal", path));
            }
            written += static_cast<std::size_t>(count);
        }
        if (!FlushFileBuffers(handle)) {
            return core::make_failure<void>(io_error("could not sync private journal", path));
        }
        return {};
    }();
#endif
    return appended;
}
} // namespace engine::platform
