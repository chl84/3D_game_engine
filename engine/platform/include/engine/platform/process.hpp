#pragma once
namespace engine::platform {
// Unknown/access-denied processes count as running: stale-lock removal fails closed.
[[nodiscard]] bool process_running(int pid) noexcept;
} // namespace engine::platform
