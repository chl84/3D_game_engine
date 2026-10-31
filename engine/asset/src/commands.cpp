#include <engine/asset/commands.hpp>

#include <engine/asset/asset_library.hpp>
#include <engine/core/path_sandbox.hpp>
#include <engine/core/sha256.hpp>
#include <engine/scene/scene.hpp>

#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <optional>
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
    schemas["asset.place@1"] = {
        {"$schema", "http://json-schema.org/draft-07/schema#"},
        {"type", "object"},
        {"additionalProperties", false},
        {"required", {"assetId"}},
        {
            "properties",
            {
                {"assetId", uuid},
                {"name", {{"type", "string"}, {"minLength", 1}}},
                {
                    "position",
                    {
                        {"type", "array"},
                        {"minItems", 3},
                        {"maxItems", 3},
                        {"items", {{"type", "number"}}},
                    },
                },
            },
        },
    };
    return schemas;
}

Result<void> register_asset_commands(command::CommandBus& bus, const std::filesystem::path& root,
                                     bool dry_run) {
    return register_asset_commands(bus, root, dry_run, false);
}

Result<void> register_asset_commands(command::CommandBus& bus, const std::filesystem::path& root,
                                     bool dry_run, bool project_sources_only) {
    const auto schemas = asset_command_schemas();
    for (const auto& [name, schema] : schemas.items()) {
        if (name == "asset.place@1") {
            continue; // Requires a scene; registered separately below.
        }
        auto added = bus.add(
            name, schema,
            [root, dry_run, project_sources_only, name,
             &bus](const json& args) -> Result<CommandOutput> {
                try {
                    if (name == "asset.import@1" || name == "asset.reimport@1") {
                        if (bus.in_transaction()) {
                            return core::make_failure<CommandOutput>(core::make_error(
                                core::ErrorCode::Unsupported,
                                "asset file imports cannot participate in an undo transaction"));
                        }
                        std::filesystem::path source;
                        if (name == "asset.import@1") {
                            source = args.at("source").get<std::string>();
                            if (project_sources_only) {
                                auto sandbox = core::PathSandbox::create(root);
                                if (!sandbox) {
                                    return core::make_failure<CommandOutput>(sandbox.error());
                                }
                                auto resolved = sandbox->resolve(source.generic_string());
                                if (!resolved) {
                                    return core::make_failure<CommandOutput>(resolved.error());
                                }
                                source = *resolved;
                            }
                        }
                        Result<ImportResult> imported =
                            name == "asset.import@1"
                                ? import_asset(root, source, args.value("name", ""), dry_run)
                                : reimport_asset(
                                      root,
                                      *core::Uuid::parse(args.at("assetId").get<std::string>()),
                                      dry_run, project_sources_only);
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

Result<void> register_asset_scene_commands(command::CommandBus& bus,
                                           const std::filesystem::path& root, scene::Scene& scene) {
    return bus.add(
        "asset.place@1", asset_command_schemas().at("asset.place@1"),
        [root, &scene](const json& args) -> Result<CommandOutput> {
            const auto asset_id = core::Uuid::parse(args.at("assetId").get<std::string>());
            if (!asset_id) {
                return core::make_failure<CommandOutput>(asset_id.error());
            }
            const auto coordinates = args.value("position", std::array<double, 3>{});
            for (double coordinate : coordinates) {
                if (!std::isfinite(coordinate)) {
                    return core::make_failure<CommandOutput>(core::make_error(
                        core::ErrorCode::Validation, "asset position must be finite"));
                }
            }
            auto loaded = load_asset(root, *asset_id);
            if (!loaded) {
                return core::make_failure<CommandOutput>(loaded.error());
            }
            const auto name = args.value("name", loaded->record.name);
            const auto asset_string = asset_id->to_string();
            const auto create = [&scene, asset_string, name,
                                 coordinates](std::optional<core::Uuid> id) -> Result<core::Uuid> {
                auto entity = scene.create_entity(name, id);
                if (!entity) {
                    return core::make_failure<core::Uuid>(entity.error());
                }
                auto added = scene.add_component<scene::AssetInstance>(*entity);
                if (!added) {
                    (void)scene.destroy_entity(*entity);
                    return core::make_failure<core::Uuid>(added.error());
                }
                scene.component<scene::AssetInstance>(*entity).asset_id = asset_string;
                scene.transform(*entity).position = {
                    coordinates[0],
                    coordinates[1],
                    coordinates[2],
                };
                return scene.uuid(*entity).value_or(core::Uuid{});
            };
            auto created = create(std::nullopt);
            if (!created) {
                return core::make_failure<CommandOutput>(created.error());
            }
            const auto id = *created;
            auto undo = [&scene, id]() -> Result<void> {
                const auto entity = scene.find(id);
                if (!entity) {
                    return core::make_failure<void>(core::make_error(
                        core::ErrorCode::NotFound, "placed asset entity no longer exists"));
                }
                return scene.destroy_entity(*entity);
            };
            auto redo = [create, id]() -> Result<void> {
                auto restored = create(id);
                if (!restored) {
                    return core::make_failure<void>(restored.error());
                }
                return {};
            };
            return CommandOutput{json{
                                     {"id", id.to_string()},
                                     {"assetId", asset_string},
                                     {"name", name},
                                     {"position", coordinates},
                                 },
                                 {},
                                 std::move(undo),
                                 std::move(redo)};
        });
}

} // namespace engine::asset
