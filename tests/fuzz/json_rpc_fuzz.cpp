#include <cstddef>
#include <cstdint>
#include <engine/control/json_rpc.hpp>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    const std::string credential(64, 't');
    const auto parsed = engine::control::parse_rpc(input, credential);
    // Authenticated envelope fuzzing reaches params/method validation without
    // passing unbounded input to a separate unguarded JSON parser.
    if (size < engine::control::kMaxRequestBytes / 2) {
        std::string wrapped =
            R"({"token":")" + credential + R"(","jsonrpc":"2.0","protocolVersion":1,)";
        wrapped.append(input);
        wrapped += '}';
        (void)engine::control::parse_rpc(wrapped, credential);
    }
    (void)parsed;
    return 0;
}
