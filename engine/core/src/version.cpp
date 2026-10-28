#include <engine/core/version.hpp>

#include <engine/core/build_info.hpp>

namespace engine::core {

std::string_view version() noexcept {
    return ENGINE_VERSION;
}

std::string_view git_commit() noexcept {
    return ENGINE_GIT_COMMIT;
}

std::string_view build_type() noexcept {
    return ENGINE_BUILD_TYPE;
}

std::string_view compiler() noexcept {
    return ENGINE_COMPILER;
}

std::string_view platform() noexcept {
    return ENGINE_PLATFORM;
}

std::string_view cxx_standard() noexcept {
    return ENGINE_CXX_STANDARD;
}

} // namespace engine::core
