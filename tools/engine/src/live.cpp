#include "commands.hpp"
#include <algorithm>
#include <array>
#include <engine/control/json_rpc.hpp>
#include <engine/core/path_sandbox.hpp>
#include <engine/platform/atomic_file.hpp>
#include <engine/platform/local_socket.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string_view>

namespace engine::cli {
namespace {
using command::CommandResult;
using core::ErrorCode;
using nlohmann::json;
int emit(const CommandResult& result, bool as_json) {
    if (as_json) {
        std::cout << result.to_json().dump() << '\n' << std::flush;
    } else if (result.ok) {
        std::cout << result.data.dump() << '\n' << std::flush;
    } else {
        std::cerr << "error: " << result.error.value_or(core::Error{}).message << '\n';
    }
    return result.ok ? 0 : core::exit_code(result.error.value_or(core::Error{}).code);
}
bool fields(const json& args, std::initializer_list<std::string_view> allowed) {
    return std::ranges::all_of(args.items(), [&](const auto& item) {
        return std::ranges::find(allowed, std::string_view(item.key())) != allowed.end();
    });
}
int dimension(const json& args, const char* name, int fallback, int maximum) {
    if (!args.contains(name)) {
        return fallback;
    }
    if (!args[name].is_number_integer()) {
        throw std::invalid_argument("integer required");
    }
    const auto number = args[name].get<std::int64_t>();
    if (number < 1 || number > maximum) {
        throw std::invalid_argument("out of range");
    }
    return static_cast<int>(number);
}
struct SessionFile {
    std::filesystem::path path;
    ~SessionFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};
} // namespace
int live_server(const std::string& root, const std::string& scene_path, int timeout_ms,
                bool as_json) {
    auto project = scene::Project::load(root);
    if (!project) {
        return emit(CommandResult::failure(project.error()), as_json);
    }
    auto sandbox = core::PathSandbox::create(project->root());
    if (!sandbox) {
        return emit(CommandResult::failure(sandbox.error()), as_json);
    }
    // Validate control paths before lock acquisition or secret creation.
    auto lock_path = sandbox->resolve(".engine/lock");
    auto token_path = sandbox->resolve(".engine/live.json");
    if (!lock_path || !token_path) {
        return emit(CommandResult::failure(!lock_path ? lock_path.error() : token_path.error()),
                    as_json);
    }
    std::error_code ec;
    if (std::filesystem::is_symlink(
            std::filesystem::symlink_status(project->root() / ".engine/live.json", ec))) {
        return emit(CommandResult::failure(
                        core::make_error(ErrorCode::Validation, "invalid live session path")),
                    as_json);
    }
    auto locked = project->lock();
    if (!locked) {
        return emit(CommandResult::failure(locked.error()), as_json);
    }
    core::ReflectionRegistry registry;
    scene::register_scene_components(registry);
    auto scene = project->load_scene(scene_path, registry);
    if (!scene) {
        return emit(CommandResult::failure(scene.error()), as_json);
    }
    command::CommandLog log;
    command::CommandBus bus(log);
    register_project_commands(bus, *project, *scene, registry, true);
    platform::LocalSocketLimits limits;
    limits.timeout_ms = timeout_ms;
    auto server = platform::LocalServer::create(limits);
    if (!server) {
        return emit(CommandResult::failure(server.error()), as_json);
    }
    auto token = platform::random_token();
    if (!token) {
        return emit(CommandResult::failure(token.error()), as_json);
    }
    const json discovery = {
        {"protocolVersion", 1},
        {"port", server->port()},
        {"token", *token},
        {"projectRoot", project->root().generic_string()},
    };
    auto published = platform::atomic_write_private_file(*token_path, discovery.dump());
    if (!published) {
        return emit(CommandResult::failure(published.error()), as_json);
    }
    const SessionFile session_file{*token_path};
    bool stopping = false;
    std::size_t logged_bytes = 0;
    (void)emit(CommandResult::success(command::CommandOutput{{
                   {"port", server->port()},
                   {"protocolVersion", 1},
                   {"sessionFile", ".engine/live.json"},
               }}),
               as_json);
    const auto handle = [&](std::string_view wire) -> std::string {
        auto parsed = control::parse_rpc(wire, *token);
        if (!parsed.request) {
            return parsed.error.dump();
        }
        const auto& request = parsed.request.value();
        const auto finish = [&](const json& result) {
            return request.notification ? std::string{} : result.dump();
        };
        const auto invalid = [&] {
            return finish(control::rpc_error(request.id, -32602, "Invalid params"));
        };
        const auto success = [&](command::CommandOutput output) {
            return finish(control::rpc_result(request.id,
                                              CommandResult::success(std::move(output)).to_json()));
        };
        const auto execute = [&](const command::CommandRequest& command) {
            if (log.entries().size() >= 4096 || logged_bytes >= std::size_t{16} * 1024 * 1024) {
                return finish(
                    control::rpc_error(request.id, -32002, "Session command limit reached"));
            }
            const auto result = bus.execute(command);
            const auto& entry = log.entries().back();
            logged_bytes += entry.arguments.dump().size() + entry.result.dump().size();
            return finish(control::rpc_result(request.id, result.to_json()));
        };
        try {
            const auto& params = request.params;
            if (request.method == "ping") {
                if (!params.empty()) {
                    return invalid();
                }
                return success(command::CommandOutput{{{"protocolVersion", 1}, {"alive", true}}});
            }
            if (request.method == "shutdown") {
                if (!params.empty()) {
                    return invalid();
                }
                stopping = true;
                return success(command::CommandOutput{{{"shutdown", true}}});
            }
            if (request.method == "command") {
                if (!fields(params, {"name", "arguments"}) || !params.contains("name") ||
                    !params["name"].is_string() ||
                    (params.contains("arguments") && !params["arguments"].is_object())) {
                    return invalid();
                }
                return execute(
                    {params["name"].get<std::string>(), params.value("arguments", json::object())});
            }
            if (request.method == "inspect") {
                if (!fields(params, {"target", "assetId", "offset", "limit"})) {
                    return invalid();
                }
                const auto target = params.value("target", "scene");
                if (target == "log") {
                    const auto offset_value = params.value("offset", json(0));
                    const auto limit_value = params.value("limit", json(32));
                    if (!offset_value.is_number_unsigned() && !offset_value.is_number_integer()) {
                        return invalid();
                    }
                    if (!limit_value.is_number_integer()) {
                        return invalid();
                    }
                    const auto offset_number = offset_value.get<std::int64_t>();
                    const auto count = limit_value.get<std::int64_t>();
                    if (offset_number < 0 || count < 1 || count > 64) {
                        return invalid();
                    }
                    const auto offset =
                        std::min(static_cast<std::size_t>(offset_number), log.entries().size());
                    json entries = json::array();
                    const auto end =
                        std::min(log.entries().size(), offset + static_cast<std::size_t>(count));
                    for (auto i = offset; i < end; ++i) {
                        const auto& entry = log.entries()[i];
                        entries.push_back({
                            {"sequence", entry.sequence},
                            {"name", entry.name},
                            {"arguments", entry.arguments},
                            {"result", entry.result},
                        });
                    }
                    return success(command::CommandOutput{
                        {{"entries", std::move(entries)}, {"total", log.entries().size()}}});
                }
                if (target == "schema") {
                    return success(command::CommandOutput{{
                        {"commands", bus.registry().schemas()},
                        {"components", registry.schemas()},
                    }});
                }
                if (target == "project") {
                    return success(command::CommandOutput{{
                        {"projectId", project->project_id().to_string()},
                        {"name", project->name()},
                        {"scenes", project->scenes()},
                    }});
                }
                command::CommandRequest command;
                if (target == "scene") {
                    command.name = "scene.inspect@1";
                } else if (target == "asset") {
                    command = {"asset.inspect@1", {{"assetId", params.at("assetId")}}};
                } else {
                    return invalid();
                }
                return execute(command);
            }
            if (request.method == "screenshot") {
                if (!fields(params, {"out", "width", "height", "frames"})) {
                    return invalid();
                }
                const auto out = params.value("out", std::string{});
                if (!out.empty()) {
                    const auto resolved = sandbox->resolve(out);
                    if (!resolved) {
                        return finish(control::rpc_result(
                            request.id, CommandResult::failure(resolved.error()).to_json()));
                    }
                    const auto relative = resolved->lexically_relative(project->root());
                    const auto first = relative.begin();
                    if (resolved->extension() != ".png" || first == relative.end() ||
                        *first == ".engine" || *first == "assets" || *first == "scenes") {
                        return invalid();
                    }
                }
                auto captured =
                    capture_scene(*project, *scene, out, dimension(params, "frames", 1, 16),
                                  dimension(params, "width", 256, 512),
                                  dimension(params, "height", 192, 512), true);
                const auto result = captured ? CommandResult::success(std::move(*captured))
                                             : CommandResult::failure(captured.error());
                return finish(control::rpc_result(request.id, result.to_json()));
            }
            return finish(control::rpc_error(request.id, -32601, "Method not found"));
        } catch (const json::exception&) {
            return invalid();
        } catch (const std::invalid_argument&) {
            return invalid();
        } catch (const std::exception&) {
            return finish(control::rpc_error(request.id, -32603, "Internal error"));
        }
    };
    auto result = server->serve(handle, [&] { return stopping; });
    if (!result) {
        std::cerr << "error: " << result.error().message << '\n';
        return core::exit_code(result.error().code);
    }
    return 0;
}
int live_call(const std::string& root, const std::string& method, const std::string& params_text,
              bool as_json) {
    auto failure = [&](core::Error error) {
        return emit(CommandResult::failure(std::move(error)), as_json);
    };
    auto project = scene::Project::load(root);
    if (!project) {
        return failure(project.error());
    }
    auto sandbox = core::PathSandbox::create(project->root());
    if (!sandbox) {
        return failure(sandbox.error());
    }
    auto path = sandbox->resolve(".engine/live.json");
    if (!path) {
        return failure(path.error());
    }
    try {
        std::error_code ec;
        if (std::filesystem::file_size(*path, ec) > 16384 || ec) {
            throw std::runtime_error("discovery unavailable");
        }
        std::ifstream file(*path, std::ios::binary);
        const auto discovery = json::parse(file);
        const auto discovery_root = discovery.at("projectRoot").get<std::string>();
        if (discovery.at("protocolVersion") != 1 ||
            !std::filesystem::equivalent(discovery_root, project->root(), ec) || ec) {
            throw std::runtime_error("invalid discovery");
        }
        if (!discovery.at("port").is_number_integer()) {
            throw std::runtime_error("invalid discovery");
        }
        const auto port = discovery.at("port").get<std::int64_t>();
        if (port < 1 || port > 65535) {
            throw std::runtime_error("invalid discovery");
        }
        const auto params = json::parse(params_text);
        if (!params.is_object()) {
            throw std::runtime_error("invalid params");
        }
        const auto wire =
            json{
                {"jsonrpc", "2.0"},     {"id", 1},
                {"method", method},     {"params", params},
                {"protocolVersion", 1}, {"token", discovery.at("token")},
            }
                .dump();
        platform::LocalSocketLimits limits;
        limits.timeout_ms = 30000;
        auto response = platform::local_request(static_cast<std::uint16_t>(port), wire, limits);
        if (!response) {
            return failure(response.error());
        }
        const auto parsed = json::parse(*response);
        if (parsed.value("jsonrpc", "") != "2.0" || parsed.at("id") != 1 ||
            !parsed.contains("result")) {
            return failure(core::make_error(ErrorCode::Precondition, "live request rejected"));
        }
        const auto& result = parsed.at("result");
        if (!result.is_object() || !result.contains("status") ||
            (result["status"] != "ok" && result["status"] != "error") || !result.contains("data") ||
            !result["data"].is_object() || !result.contains("warnings") ||
            !result["warnings"].is_array()) {
            throw std::runtime_error("invalid response");
        }
        int exit_status = 0;
        if (result["status"] == "error") {
            const auto code = result.at("error").at("code").get<std::string>();
            constexpr std::array codes{
                ErrorCode::Usage,        ErrorCode::Validation, ErrorCode::NotFound,
                ErrorCode::Conflict,     ErrorCode::Io,         ErrorCode::Unsupported,
                ErrorCode::Precondition, ErrorCode::Internal,   ErrorCode::Cancelled,
            };
            const auto* const found = std::ranges::find_if(
                codes, [&](auto value) { return code == core::error_code_name(value); });
            if (found == codes.end()) {
                throw std::runtime_error("invalid response");
            }
            exit_status = core::exit_code(*found);
        }
        if (as_json) {
            std::cout << result.dump() << '\n';
        } else {
            std::cout << result.at("data").dump() << '\n';
        }
        return exit_status;
    } catch (const std::exception&) {
        return failure(core::make_error(ErrorCode::Validation, "invalid live session or request"));
    }
}
} // namespace engine::cli
