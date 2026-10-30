#include <engine/platform/local_socket.hpp>
#include <iostream>
#include <string>

// A process-level fixture keeps the production single-threaded dispatch model.
// stdin gates let the client arrange ready sockets without timing races.
int main() {
    engine::platform::LocalSocketLimits limits;
    limits.timeout_ms = 1000;
    auto server = engine::platform::LocalServer::create(limits);
    if (!server) {
        return 1;
    }
    std::cout << server->port() << '\n' << std::flush;
    bool stopped = false;
    const auto served = server->serve(
        [&](std::string_view frame) {
            std::cout << frame << '\n' << std::flush;
            if (frame == "gate" || frame == "slow") {
                std::string release;
                if (!std::getline(std::cin, release)) {
                    stopped = true;
                }
            }
            if (frame == "shutdown") {
                stopped = true;
            }
            return std::string("ok");
        },
        [&] { return stopped; });
    return served ? 0 : 1;
}
