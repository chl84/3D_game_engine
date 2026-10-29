#include <engine/core/path_sandbox.hpp>

#include <algorithm>
#include <string>
#include <system_error>
#include <utility>

namespace engine::core {
namespace {

[[nodiscard]] bool has_parent_component(const std::filesystem::path& path) {
    return std::ranges::any_of(path, [](const auto& part) { return part == ".."; });
}

[[nodiscard]] bool is_within(const std::filesystem::path& root,
                             const std::filesystem::path& candidate) {
    const auto relative = candidate.lexically_relative(root);
    if (relative.empty()) {
        return candidate == root;
    }
    return *relative.begin() != "..";
}

} // namespace

Result<PathSandbox> PathSandbox::create(const std::filesystem::path& root) {
    std::error_code error;
    const auto canonical = std::filesystem::canonical(root, error);
    if (error) {
        return make_failure<PathSandbox>(
            make_error(ErrorCode::Io, "Project root does not exist or cannot be resolved.", {},
                       {{"path", root.string()}}));
    }

    if (!std::filesystem::is_directory(canonical, error) || error) {
        return make_failure<PathSandbox>(make_error(ErrorCode::Validation,
                                                    "Project root is not a directory.", {},
                                                    {{"path", canonical.string()}}));
    }

    return PathSandbox(canonical);
}

PathSandbox::PathSandbox(std::filesystem::path root) : root_(std::move(root)) {}

const std::filesystem::path& PathSandbox::root() const noexcept {
    return root_;
}

Result<std::filesystem::path> PathSandbox::resolve(std::string_view relative) const {
    if (relative.empty()) {
        return make_failure<std::filesystem::path>(make_error(ErrorCode::Validation,
                                                              "Path must not be empty.", {},
                                                              {{"path", std::string(relative)}}));
    }

    std::filesystem::path path;
    try {
        path = std::filesystem::path(std::string(relative));
    } catch (const std::exception& error) {
        return make_failure<std::filesystem::path>(make_error(ErrorCode::Validation,
                                                              "Path is not valid.", error.what(),
                                                              {{"path", std::string(relative)}}));
    }

    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
        return make_failure<std::filesystem::path>(make_error(ErrorCode::Validation,
                                                              "Absolute paths are not allowed.", {},
                                                              {{"path", std::string(relative)}}));
    }

    if (has_parent_component(path)) {
        return make_failure<std::filesystem::path>(
            make_error(ErrorCode::Validation, "Parent path components are not allowed.", {},
                       {{"path", std::string(relative)}}));
    }

    std::error_code error;
    auto candidate = std::filesystem::weakly_canonical(root_ / path, error);
    if (error) {
        return make_failure<std::filesystem::path>(
            make_error(ErrorCode::Io, "Path cannot be resolved.", error.message(),
                       {{"path", std::string(relative)}}));
    }

    if (!is_within(root_, candidate)) {
        return make_failure<std::filesystem::path>(make_error(ErrorCode::Validation,
                                                              "Path escapes the project root.", {},
                                                              {{"path", std::string(relative)}}));
    }

    return candidate;
}

} // namespace engine::core
