#pragma once

#include <engine/core/uuid.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace engine::test {

class TempDir {
public:
    explicit TempDir(std::string_view name) {
        path_ = std::filesystem::temp_directory_path() /
                ("engine_" + std::string(name) + "_" + engine::core::Uuid::generate().to_string());

        std::error_code error;
        const bool created = std::filesystem::create_directories(path_, error);
        if (!created || error) {
            throw std::runtime_error("failed to create temporary directory: " + path_.string());
        }
    }

    ~TempDir() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

} // namespace engine::test
