#pragma once

#include <engine/core/error.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::command {

using core::Error;
using core::Result;

struct CommandRequest {
    std::string name;
    nlohmann::json arguments{nlohmann::json::object()};
};

struct CommandOutput {
    nlohmann::json data{nlohmann::json::object()};
    std::vector<std::string> warnings;
};

struct CommandResult {
    bool ok{false};
    nlohmann::json data{nlohmann::json::object()};
    std::optional<Error> error;
    std::vector<std::string> warnings;

    [[nodiscard]] static CommandResult success(CommandOutput output);
    [[nodiscard]] static CommandResult failure(Error error);
    [[nodiscard]] nlohmann::json to_json() const;
};

struct CommandLogEntry {
    std::uint64_t sequence{0};
    std::string name;
    nlohmann::json arguments{nlohmann::json::object()};
    nlohmann::json result{nlohmann::json::object()};
};

class CommandLog {
public:
    void append(std::string name, nlohmann::json arguments, nlohmann::json result);

    [[nodiscard]] const std::vector<CommandLogEntry>& entries() const noexcept;
    [[nodiscard]] nlohmann::json to_json() const;
    void clear() noexcept;

private:
    std::vector<CommandLogEntry> entries_;
    std::uint64_t next_sequence_{1};
};

class CommandRegistry {
public:
    using Handler = std::function<Result<CommandOutput>(const nlohmann::json&)>;

    [[nodiscard]] Result<void> add(std::string name, nlohmann::json schema, Handler handler);
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] const nlohmann::json* schema(std::string_view name) const;
    [[nodiscard]] nlohmann::json schemas() const;

    [[nodiscard]] CommandResult execute(const CommandRequest& request) const;

private:
    struct Entry {
        nlohmann::json schema;
        std::shared_ptr<nlohmann::json_schema::json_validator> validator;
        Handler handler;
    };

    std::unordered_map<std::string, Entry> commands_;
};

class CommandBus {
public:
    explicit CommandBus(CommandLog& log);

    [[nodiscard]] Result<void> add(std::string name, nlohmann::json schema,
                                   CommandRegistry::Handler handler);
    [[nodiscard]] CommandResult execute(const CommandRequest& request);
    [[nodiscard]] const CommandRegistry& registry() const noexcept;
    [[nodiscard]] const CommandLog& log() const noexcept;

private:
    CommandRegistry registry_;
    CommandLog& log_;
};

} // namespace engine::command
