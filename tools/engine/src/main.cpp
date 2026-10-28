#include <engine/core/version.hpp>

#include <iostream>
#include <string>
#include <string_view>

namespace {

std::string json_quote(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (const char c : value) {
        switch (c) {
        case '"':
        case '\\':
            out.push_back('\\');
            out.push_back(c);
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    out.push_back('"');
    return out;
}

void print_usage(std::ostream& out) {
    out << "Usage: engine [--version] [--json]\n"
        << "\n"
        << "  --version, -v  Print the engine version.\n"
        << "  --json         Emit a machine-readable JSON document.\n"
        << "  --help, -h     Print this help.\n";
}

void print_version_json() {
    std::cout << '{'
              << "\"name\":" << json_quote("engine") << ','
              << "\"version\":" << json_quote(engine::core::version()) << ','
              << "\"gitCommit\":" << json_quote(engine::core::git_commit()) << ','
              << "\"buildType\":" << json_quote(engine::core::build_type()) << ','
              << "\"compiler\":" << json_quote(engine::core::compiler()) << ','
              << "\"platform\":" << json_quote(engine::core::platform()) << ','
              << "\"cxxStandard\":" << json_quote(engine::core::cxx_standard())
              << "}\n";
}

void print_version_human() {
    std::cout << "engine " << engine::core::version()
              << " (" << engine::core::git_commit()
              << ", " << engine::core::build_type()
              << ", " << engine::core::compiler()
              << ", " << engine::core::platform()
              << ")\n";
}

} // namespace

int main(int argc, char** argv) {
    bool show_version = false;
    bool json = false;
    bool help = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            show_version = true;
        } else if (arg == "--json") {
            json = true;
        } else if (arg == "--help" || arg == "-h") {
            help = true;
        } else {
            std::cerr << "error: unknown argument: " << arg << '\n';
            print_usage(std::cerr);
            return 2;
        }
    }

    if (help) {
        print_usage(std::cout);
        return 0;
    }

    if (!show_version) {
        print_usage(std::cerr);
        return 2;
    }

    if (json) {
        print_version_json();
    } else {
        print_version_human();
    }

    return 0;
}
