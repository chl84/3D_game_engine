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
#include <utility>
#include <vector>

namespace engine::command {

using core::Error;
using core::Result;

struct CommandRequest {
    std::string name;
    nlohmann::json arguments = nlohmann::json::object();
};

using UndoAction = std::function<Result<void>()>;

struct CommandOutput {
    nlohmann::json data = nlohmann::json::object();
    std::vector<std::string> warnings;
    UndoAction undo;
    UndoAction redo;

    CommandOutput() = default;
    explicit CommandOutput(nlohmann::json output_data,
                           std::vector<std::string> output_warnings = {},
                           UndoAction undo_action = {}, UndoAction redo_action = {})
        : data(std::move(output_data)), warnings(std::move(output_warnings)),
          undo(std::move(undo_action)), redo(std::move(redo_action)) {}
};

struct CommandResult {
    bool ok{false};
    nlohmann::json data = nlohmann::json::object();
    std::optional<Error> error;
    std::vector<std::string> warnings;
    UndoAction undo;
    UndoAction redo;

    [[nodiscard]] static CommandResult success(CommandOutput output);
    [[nodiscard]] static CommandResult failure(Error error);
    [[nodiscard]] nlohmann::json to_json() const;
};

struct CommandLogEntry {
    std::uint64_t sequence{0};
    std::string name;
    nlohmann::json arguments = nlohmann::json::object();
    nlohmann::json result = nlohmann::json::object();
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

    [[nodiscard]] Result<void> begin_transaction();
    [[nodiscard]] Result<void> commit_transaction();
    [[nodiscard]] Result<void> rollback_transaction();
    [[nodiscard]] bool in_transaction() const noexcept;

    [[nodiscard]] Result<void> clear_history();
    [[nodiscard]] Result<void> undo();
    [[nodiscard]] Result<void> redo();
    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;

    [[nodiscard]] const CommandRegistry& registry() const noexcept;
    [[nodiscard]] const CommandLog& log() const noexcept;

private:
    struct HistoryEntry {
        std::string name;
        UndoAction undo;
        UndoAction redo;
    };

    CommandRegistry registry_;
    CommandLog& log_;
    std::vector<HistoryEntry> undo_stack_;
    std::vector<HistoryEntry> redo_stack_;
    std::vector<HistoryEntry> transaction_entries_;
    bool transaction_active_{false};
};

} // namespace engine::command
