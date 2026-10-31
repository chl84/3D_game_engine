#pragma once
#include <cstddef>
#include <cstdint>
#include <engine/core/error.hpp>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace engine::platform {
struct LocalSocketLimits {
    std::size_t request_bytes{65536};
    std::size_t reply_bytes{std::size_t{4} * 1024U * 1024U};
    std::size_t connections{8};
    int timeout_ms{2000};
};
// One newline-delimited frame per connection. All I/O is nonblocking; incomplete,
// oversized, timed-out and excess connections close without dispatching a frame.
class LocalServer {
public:
    ~LocalServer();
    LocalServer(LocalServer&&) noexcept;
    LocalServer& operator=(LocalServer&&) noexcept;
    [[nodiscard]] static core::Result<LocalServer> create(LocalSocketLimits limits = {});
    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] core::Result<void>
    serve(const std::function<std::string(std::string_view)>& handle,
          const std::function<bool()>& stopping);

private:
    struct Impl;
    explicit LocalServer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
[[nodiscard]] core::Result<std::string> local_request(std::uint16_t port, std::string_view frame,
                                                      LocalSocketLimits limits = {});
[[nodiscard]] core::Result<std::string> random_token();
} // namespace engine::platform
