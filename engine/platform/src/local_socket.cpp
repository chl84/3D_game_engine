#include <algorithm>
#include <array>
#include <chrono>
#include <engine/platform/local_socket.hpp>
#include <limits>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>

#include <windows.h>

#include <bcrypt.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace engine::platform {
namespace {
using Clock = std::chrono::steady_clock;
#ifdef _WIN32
using Native = SOCKET;
constexpr Native kInvalid = INVALID_SOCKET;
void close_socket(Native s) {
    if (s != kInvalid) {
        (void)closesocket(s);
    }
}
bool would_block() {
    const int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
}
bool nonblocking(Native s) {
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0 &&
           SetHandleInformation(reinterpret_cast<HANDLE>(s), HANDLE_FLAG_INHERIT, 0) != 0;
}
struct Network {
    bool ready{false};
    Network() {
        WSADATA data{};
        ready = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }
    ~Network() {
        if (ready) {
            (void)WSACleanup();
        }
    }
};
#else
using Native = int;
constexpr Native kInvalid = -1;
void close_socket(Native s) {
    if (s != kInvalid) {
        (void)::close(s);
    }
}
bool would_block() {
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == EINTR;
}
bool nonblocking(Native s) {
    return s < FD_SETSIZE && fcntl(s, F_SETFL, O_NONBLOCK) == 0 &&
           fcntl(s, F_SETFD, FD_CLOEXEC) == 0;
}
struct Network {
    bool ready{true};
};
#endif
struct Socket {
    Native value{kInvalid};
    explicit Socket(Native socket = kInvalid) : value(socket) {}
    ~Socket() {
        close_socket(value);
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept : value(std::exchange(other.value, kInvalid)) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close_socket(value);
            value = std::exchange(other.value, kInvalid);
        }
        return *this;
    }
};
core::Error net_error() {
    return core::make_error(core::ErrorCode::Io, "local connection failed or timed out");
}
int receive(Native socket, char* data, std::size_t size) {
#ifdef _WIN32
    return ::recv(socket, data, static_cast<int>(size), 0);
#else
    return static_cast<int>(::recv(socket, data, size, 0));
#endif
}
int transmit(Native socket, const char* data, std::size_t size) {
#ifdef _WIN32
    constexpr int flags = 0;
#else
    constexpr int flags = MSG_NOSIGNAL;
#endif
#ifdef _WIN32
    return ::send(socket, data, static_cast<int>(size), flags);
#else
    return static_cast<int>(::send(socket, data, size, flags));
#endif
}
int wait_sockets(fd_set& read, fd_set& write, Native maximum, int milliseconds) {
    timeval timeout{};
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = static_cast<decltype(timeout.tv_usec)>(milliseconds % 1000) * 1000;
#ifdef _WIN32
    (void)maximum;
    return ::select(0, &read, &write, nullptr, &timeout);
#else
    return ::select(maximum + 1, &read, &write, nullptr, &timeout);
#endif
}
bool valid_limits(const LocalSocketLimits& limits) {
    return limits.request_bytes > 0 && limits.request_bytes <= 65536 && limits.reply_bytes > 0 &&
           limits.reply_bytes <= std::size_t{4} * 1024U * 1024U && limits.connections > 0 &&
           limits.connections <= 8 && limits.timeout_ms >= 50 && limits.timeout_ms <= 30000;
}
} // namespace
struct LocalServer::Impl {
    Network network;
    Socket listener;
    LocalSocketLimits limits;
    std::uint16_t port{0};
};
LocalServer::LocalServer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
LocalServer::~LocalServer() = default;
LocalServer::LocalServer(LocalServer&&) noexcept = default;
LocalServer& LocalServer::operator=(LocalServer&&) noexcept = default;
core::Result<LocalServer> LocalServer::create(LocalSocketLimits limits) {
    auto impl = std::make_unique<Impl>();
    if (!valid_limits(limits) || !impl->network.ready) {
        return core::make_failure<LocalServer>(net_error());
    }
    impl->limits = limits;
    impl->listener = Socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (impl->listener.value == kInvalid || !nonblocking(impl->listener.value)) {
        return core::make_failure<LocalServer>(net_error());
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(impl->listener.value, reinterpret_cast<const sockaddr*>(&address),
               sizeof(address)) != 0 ||
        ::listen(impl->listener.value, static_cast<int>(limits.connections)) != 0) {
        return core::make_failure<LocalServer>(net_error());
    }
#ifdef _WIN32
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif
    if (getsockname(impl->listener.value, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return core::make_failure<LocalServer>(net_error());
    }
    impl->port = ntohs(address.sin_port);
    return LocalServer(std::move(impl));
}
std::uint16_t LocalServer::port() const noexcept {
    return impl_->port;
}
core::Result<void> LocalServer::serve(const std::function<std::string(std::string_view)>& handle,
                                      const std::function<bool()>& stopping) {
    struct Client {
        Socket socket;
        std::string input;
        std::string output;
        std::size_t sent{0};
        Clock::time_point deadline;
    };
    std::vector<Client> clients;
    auto& p = *impl_;
    for (;;) {
        std::erase_if(clients, [](const Client& c) {
            return c.socket.value == kInvalid || Clock::now() >= c.deadline;
        });
        if (stopping()) {
            std::erase_if(clients, [](const Client& c) { return c.output.empty(); });
            if (clients.empty()) {
                return {};
            }
        }
        fd_set read{};
        fd_set write{};
        FD_ZERO(&read);
        FD_ZERO(&write);
        Native maximum = p.listener.value;
        if (!stopping()) {
            FD_SET(p.listener.value, &read);
        }
        for (const auto& c : clients) {
            if (c.output.empty()) {
                FD_SET(c.socket.value, &read);
            } else {
                FD_SET(c.socket.value, &write);
            }
            maximum = std::max(maximum, c.socket.value);
        }
        if (wait_sockets(read, write, maximum, 20) < 0) {
            if (would_block()) {
                continue;
            }
            return core::make_failure<void>(net_error());
        }
        for (auto& c : clients) {
            if (!c.output.empty() && FD_ISSET(c.socket.value, &write)) {
                const auto left = c.output.size() - c.sent;
                const int count = transmit(c.socket.value, c.output.data() + c.sent,
                                           std::min<std::size_t>(left, 65536));
                if (count > 0) {
                    c.sent += static_cast<std::size_t>(count);
                } else if (count == 0 || !would_block()) {
                    c.socket = Socket();
                }
                if (c.sent == c.output.size()) {
                    c.socket = Socket();
                }
            } else if (c.output.empty() && FD_ISSET(c.socket.value, &read)) {
                if (stopping()) {
                    c.socket = Socket();
                    continue;
                }
                std::array<char, 4096> buffer{};
                const int count = receive(c.socket.value, buffer.data(), buffer.size());
                if (count <= 0) {
                    if (count == 0 || !would_block()) {
                        c.socket = Socket();
                    }
                    continue;
                }
                c.input.append(buffer.data(), static_cast<std::size_t>(count));
                const auto newline = c.input.find('\n');
                if (c.input.size() > p.limits.request_bytes + 1 ||
                    (newline == std::string::npos && c.input.size() > p.limits.request_bytes)) {
                    c.socket = Socket();
                    continue;
                }
                if (newline != std::string::npos) {
                    if (newline + 1 != c.input.size()) {
                        c.socket = Socket();
                        continue;
                    }
                    c.input.resize(newline);
                    c.output = handle(c.input);
                    if (c.output.empty() || c.output.size() > p.limits.reply_bytes) {
                        c.socket = Socket();
                        continue;
                    }
                    c.output.push_back('\n');
                    c.deadline = Clock::now() + std::chrono::milliseconds(p.limits.timeout_ms);
                }
            }
        }
        if (!stopping() && FD_ISSET(p.listener.value, &read)) {
            sockaddr_in peer{};
#ifdef _WIN32
            int size = sizeof(peer);
#else
            socklen_t size = sizeof(peer);
#endif
            Socket incoming(::accept(p.listener.value, reinterpret_cast<sockaddr*>(&peer), &size));
            std::erase_if(clients, [](const Client& c) { return c.socket.value == kInvalid; });
            if (incoming.value != kInvalid && clients.size() < p.limits.connections &&
                peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && nonblocking(incoming.value)) {
                clients.push_back({
                    std::move(incoming),
                    {},
                    {},
                    0,
                    Clock::now() + std::chrono::milliseconds(p.limits.timeout_ms),
                });
            }
        }
    }
}
core::Result<std::string> local_request(std::uint16_t port, std::string_view frame,
                                        LocalSocketLimits limits) {
    Network network;
    if (!network.ready || !valid_limits(limits) || frame.size() > limits.request_bytes) {
        return core::make_failure<std::string>(net_error());
    }
    Socket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socket.value == kInvalid || !nonblocking(socket.value)) {
        return core::make_failure<std::string>(net_error());
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::connect(socket.value, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) !=
            0 &&
        !would_block()) {
        return core::make_failure<std::string>(net_error());
    }
    std::string output(frame);
    output.push_back('\n');
    std::string input;
    std::size_t sent = 0;
    const auto deadline = Clock::now() + std::chrono::milliseconds(limits.timeout_ms);
    while (Clock::now() < deadline) {
        fd_set read{};
        fd_set write{};
        FD_ZERO(&read);
        FD_ZERO(&write);
        if (sent < output.size()) {
            FD_SET(socket.value, &write);
        } else {
            FD_SET(socket.value, &read);
        }
        if (wait_sockets(read, write, socket.value, 20) < 0) {
            return core::make_failure<std::string>(net_error());
        }
        if (FD_ISSET(socket.value, &write)) {
            const int count = transmit(socket.value, output.data() + sent,
                                       std::min<std::size_t>(output.size() - sent, 65536));
            if (count > 0) {
                sent += static_cast<std::size_t>(count);
            } else if (count == 0 || !would_block()) {
                return core::make_failure<std::string>(net_error());
            }
        }
        if (FD_ISSET(socket.value, &read)) {
            std::array<char, 4096> buffer{};
            const int count = receive(socket.value, buffer.data(), buffer.size());
            if (count <= 0) {
                if (count < 0 && would_block()) {
                    continue;
                }
                break;
            }
            input.append(buffer.data(), static_cast<std::size_t>(count));
            if (input.size() > limits.reply_bytes + 1) {
                break;
            }
            const auto end = input.find('\n');
            if (end != std::string::npos) {
                input.resize(end);
                return input;
            }
        }
    }
    return core::make_failure<std::string>(net_error());
}
core::Result<std::string> random_token() {
    std::array<unsigned char, 32> bytes{};
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return core::make_failure<std::string>(net_error());
    }
#else
    const int fd = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return core::make_failure<std::string>(net_error());
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::read(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            (void)::close(fd);
            return core::make_failure<std::string>(net_error());
        }
        offset += static_cast<std::size_t>(count);
    }
    (void)::close(fd);
#endif
    constexpr std::string_view hex = "0123456789abcdef";
    std::string result;
    for (unsigned char byte : bytes) {
        result += hex[byte >> 4U];
        result += hex[byte & 15U];
    }
    return result;
}
} // namespace engine::platform
