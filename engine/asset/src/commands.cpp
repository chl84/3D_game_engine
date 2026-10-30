#include <engine/asset/commands.hpp>

#include <engine/asset/asset_library.hpp>
#include <engine/core/path_sandbox.hpp>
#include <engine/core/sha256.hpp>

#include <array>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace engine::asset {
namespace {

using command::CommandOutput;
using core::Result;
using nlohmann::json;

json record_json(const AssetRecord& record) {
    return {
        {"assetId", record.asset_id.to_string()},
        {"name", record.name},
        {"sourceCopy", record.source_copy},
        {"sourceHash", record.source_hash},
        {"cacheKey", record.cache_key},
        {"cachePath", record.cache_path},
        {"animationChannelCount", record.animation_channel_count},
    };
}

Result<CommandOutput> inspect(const std::filesystem::path& root, const core::Uuid& id,
                              bool validate) {
    auto manifest = inspect_asset(root, id);
    if (!manifest) {
        return core::make_failure<CommandOutput>(manifest.error());
    }
    auto record = read_asset_record(root, id);
    if (!record) {
        return core::make_failure<CommandOutput>(record.error());
    }
    auto loaded = load_asset(root, id);
    if (validate && !loaded) {
        return core::make_failure<CommandOutput>(loaded.error());
    }
    if (validate) {
        auto sandbox = core::PathSandbox::create(root);
        if (!sandbox) {
            return core::make_failure<CommandOutput>(sandbox.error());
        }
        auto source = sandbox->resolve(record->source_copy);
        if (!source) {
            return core::make_failure<CommandOutput>(source.error());
        }
        std::error_code ec;
        const auto size = std::filesystem::file_size(*source, ec);
        if (ec || size > kMaxSourceBytes) {
            return core::make_failure<CommandOutput>(
                core::make_error(core::ErrorCode::Validation,
                                 "asset source copy is missing or exceeds the source limit"));
        }
        std::ifstream input(*source, std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(input),
                                std::istreambuf_iterator<char>()};
        if (!input || bytes.size() != size || core::sha256_hex(bytes) != record->source_hash) {
            return core::make_failure<CommandOutput>(core::make_error(
                core::ErrorCode::Validation, "asset source-copy hash does not match the sidecar",
                "Reimport the asset."));
        }
    }
    json data = record_json(*record);
    data["meshes"] = manifest->at("meshes");
    data["materials"] = manifest->at("materials");
    data["textures"] = manifest->at("textures");
    data["nodes"] = manifest->at("nodes");
    // Original source paths are local reimport details. Machine-facing results
    // use the portable project copy, including dependency records.
    data["dependencies"] = json::array(
        {{{"kind", "source"}, {"path", record->source_copy}, {"hash", record->source_hash}}});
    data["cache"] = {{"state", loaded ? "ready" : "unavailable"}};
    std::vector<std::string> warnings;
    if (!loaded) {
        data["cache"]["errorCode"] = core::error_code_name(loaded.error().code);
        warnings.emplace_back("Cooked cache is unavailable; reimport the asset to rebuild it.");
    }
    if (record->animation_channel_count != 0U) {
        warnings.emplace_back("Animation channels are present but are not imported in M3.");
    }
    if (validate) {
        data["valid"] = true;
    }
    return CommandOutput{std::move(data), std::move(warnings)};
}

} // namespace

json asset_command_schemas() {
    const json uuid = {
        {"type", "string"},
        {"pattern", "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"},
    };
    json schemas = json::object();
    for (const auto* name : {"import", "inspect", "validate", "reimport", "list"}) {
        json properties = json::object();
        json required = json::array();
        if (std::string_view(name) == "import") {
            properties["source"] = {{"type", "string"}, {"minLength", 1}};
            properties["name"] = {{"type", "string"}, {"minLength", 1}};
            required.push_back("source");
        } else if (std::string_view(name) != "list") {
            properties["assetId"] = uuid;
            required.push_back("assetId");
        }
        schemas["asset." + std::string(name) + "@1"] = {
            {"$schema", "http://json-schema.org/draft-07/schema#"},
            {"type", "object"},
            {"additionalProperties", false},
            {"properties", std::move(properties)},
            {"required", std::move(required)},
        };
    }
    return schemas;
}

Result<void> register_asset_commands(command::CommandBus& bus, const std::filesystem::path& root,
                                     bool dry_run) {
    const auto schemas = asset_command_schemas();
    for (const auto& [name, schema] : schemas.items()) {
        auto added = bus.add(
            name, schema, [root, dry_run, name, &bus](const json& args) -> Result<CommandOutput> {
                try {
                    if (name == "asset.import@1" || name == "asset.reimport@1") {
                        if (bus.in_transaction()) {
                            return core::make_failure<CommandOutput>(core::make_error(
                                core::ErrorCode::Unsupported,
                                "asset file imports cannot participate in an undo transaction"));
                        }
                        Result<ImportResult> imported =
                            name == "asset.import@1"
                                ? import_asset(root, args.at("source").get<std::string>(),
                                               args.value("name", ""), dry_run)
                                : reimport_asset(
                                      root,
                                      *core::Uuid::parse(args.at("assetId").get<std::string>()),
                                      dry_run);
                        if (!imported) {
                            return core::make_failure<CommandOutput>(imported.error());
                        }
                        json data = record_json(imported->record);
                        data["cacheHit"] = imported->cache_hit;
                        data["dryRun"] = dry_run;
                        if (dry_run && name == "asset.import@1") {
                            // Preview IDs are disposable, not reservations.
                            data["preview"] = true;
                        }
                        return CommandOutput{std::move(data)};
                    }
                    if (name == "asset.list@1") {
                        auto records = list_assets(root);
                        if (!records) {
                            return core::make_failure<CommandOutput>(records.error());
                        }
                        json assets = json::array();
                        for (const auto& record : *records) {
                            assets.push_back(record_json(record));
                        }
                        return CommandOutput{json{{"assets", std::move(assets)}}};
                    }
                    auto id = core::Uuid::parse(args.at("assetId").get<std::string>());
                    if (!id) {
                        return core::make_failure<CommandOutput>(id.error());
                    }
                    return inspect(root, *id, name == "asset.validate@1");
                } catch (const std::exception& error) {
                    return core::make_failure<CommandOutput>(
                        core::make_error(core::ErrorCode::Validation,
                                         "asset command could not read its input", error.what()));
                }
            });
        if (!added) {
            return added;
        }
    }
    return {};
}

} // namespace engine::asset
