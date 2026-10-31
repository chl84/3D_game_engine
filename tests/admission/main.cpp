#include "admission.hpp"

#include <charconv>
#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    try {
        admission::require(argc >= 2,
                           "expected --verify-lua, --verify-jolt or --benchmark MODEL COUNT");
        const std::string_view command(argv[1]);
        nlohmann::json result;
#ifdef M4_JOLT_PROBE_ONLY
        if (command == "--verify-jolt" && argc == 2) {
            result = admission::verify_jolt();
        } else {
            throw std::runtime_error("expected --verify-jolt");
        }
#else
        if (command == "--verify-lua" && argc == 2) {
            result = admission::verify_lua();
        } else if (command == "--benchmark" && argc == 4) {
            std::size_t count = 0;
            const std::string_view text(argv[3]);
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), count);
            admission::require(parsed.ec == std::errc{} &&
                                   parsed.ptr == text.data() + text.size() && count > 0 &&
                                   count <= 1000,
                               "instance count must be in 1..1000");
            result = admission::benchmark_lua(argv[2], count);
        } else {
            throw std::runtime_error("invalid admission command");
        }
#endif
        result["status"] = "ok";
        result["compiler"] = M4_PROBE_COMPILER;
        result["buildType"] = M4_PROBE_BUILD_TYPE;
        result["ownCodeWarningsAsErrors"] = true;
        std::cout << result.dump(2) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M4 admission failed: " << error.what() << '\n';
        return 1;
    }
}
