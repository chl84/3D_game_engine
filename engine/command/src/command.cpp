#include <engine/command/command.hpp>

#include <algorithm>
#include <regex>
#include <utility>

namespace engine::command {
namespace {

[[nodiscard]] const std::regex& command_name_pattern() {
    static const std::regex pattern(R"(^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)*@[1-9][0-9]*$)");
    return pattern;
}

} // namespace

CommandResult CommandResult::success(CommandOutput output) {
    return CommandResult{true, std::move(output.data), std::nullopt, std::move(output.warnings)};
}

CommandResult CommandResult::failure(Error error) {
    return CommandResult{false, nlohmann::json::object(), std::move(error), {}};
}

nlohmann::json CommandResult::to_json() const {
    nlohmann::json result{
        {"status", ok ? "ok" : "error"},
        {"data", data},
        {"warnings", warnings},
    };
    if (error.has_value()) {
        result["error"] = error->to_json();
    }
    return result;
}

void CommandLog::append(std::string name, nlohmann::json arguments, nlohmann::json result) {
    entries_.push_back(CommandLogEntry{
        next_sequence_++,
        std::move(name),
        std::move(arguments),
        std::move(result),
    });
}

const std::vector<CommandLogEntry>& CommandLog::entries() const noexcept {
    return entries_;
}

nlohmann::json CommandLog::to_json() const {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& entry : entries_) {
        result.push_back({
            {"sequence", entry.sequence},
            {"name", entry.name},
            {"arguments", entry.arguments},
            {"result", entry.result},
        });
    }
    return result;
}

void CommandLog::clear() noexcept {
    entries_.clear();
    next_sequence_ = 1;
}

Result<void> CommandRegistry::add(std::string name, nlohmann::json schema, Handler handler) {
    if (!std::regex_match(name, command_name_pattern())) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Validation, "invalid command name",
            "Use a versioned name such as scene.entity.create@1.", {{"name", name}}));
    }
    if (commands_.contains(name)) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Conflict, "command is already registered", {}, {{"name", name}}));
    }
    if (!schema.is_object()) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Validation, "command schema must be an object", {}, {{"name", name}}));
    }

    auto validator = std::make_shared<nlohmann::json_schema::json_validator>();
    try {
        validator->set_root_schema(schema);
    } catch (const std::exception& error) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Validation, "invalid command schema", error.what(), {{"name", name}}));
    }

    commands_.emplace(std::move(name),
                      Entry{std::move(schema), std::move(validator), std::move(handler)});
    return {};
}

std::vector<std::string> CommandRegistry::names() const {
    std::vector<std::string> result;
    result.reserve(commands_.size());
    for (const auto& [name, entry] : commands_) {
        (void)entry;
        result.push_back(name);
    }
    std::ranges::sort(result);
    return result;
}

const nlohmann::json* CommandRegistry::schema(std::string_view name) const {
    const auto iterator = commands_.find(std::string(name));
    return iterator == commands_.end() ? nullptr : &iterator->second.schema;
}

nlohmann::json CommandRegistry::schemas() const {
    nlohmann::json result = nlohmann::json::object();
    for (const auto& [name, entry] : commands_) {
        result[name] = entry.schema;
    }
    return result;
}

CommandResult CommandRegistry::execute(const CommandRequest& request) const {
    const auto iterator = commands_.find(request.name);
    if (iterator == commands_.end()) {
        return CommandResult::failure(core::make_error(core::ErrorCode::NotFound, "unknown command",
                                                       {}, {{"name", request.name}}));
    }

    try {
        iterator->second.validator->validate(request.arguments);
    } catch (const std::exception& error) {
        return CommandResult::failure(core::make_error(core::ErrorCode::Validation,
                                                       "command arguments are invalid",
                                                       error.what(), {{"name", request.name}}));
    }

    auto result = iterator->second.handler(request.arguments);
    if (!result) {
        return CommandResult::failure(std::move(result.error()));
    }
    return CommandResult::success(std::move(*result));
}

CommandBus::CommandBus(CommandLog& log) : log_(log) {}

Result<void> CommandBus::add(std::string name, nlohmann::json schema,
                             CommandRegistry::Handler handler) {
    return registry_.add(std::move(name), std::move(schema), std::move(handler));
}

CommandResult CommandBus::execute(const CommandRequest& request) {
    CommandResult result = registry_.execute(request);
    log_.append(request.name, request.arguments, result.to_json());
    return result;
}

const CommandRegistry& CommandBus::registry() const noexcept {
    return registry_;
}

const CommandLog& CommandBus::log() const noexcept {
    return log_;
}

} // namespace engine::command
