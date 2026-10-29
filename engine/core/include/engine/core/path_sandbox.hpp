#pragma once

#include <filesystem>
#include <string_view>

#include <engine/core/error.hpp>

namespace engine::core {

class PathSandbox {
public:
    [[nodiscard]] static Result<PathSandbox> create(const std::filesystem::path& root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] Result<std::filesystem::path> resolve(std::string_view relative) const;

private:
    explicit PathSandbox(std::filesystem::path root);

    std::filesystem::path root_;
};

} // namespace engine::core
