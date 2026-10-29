#include <engine/scene/project.hpp>

#include <engine/core/path_sandbox.hpp>
#include <engine/platform/atomic_file.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <initializer_list>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace engine::scene {
namespace {

constexpr int kFormatVersion = 1;

[[nodiscard]] Error project_error(ErrorCode code, std::string message,
                                  const std::filesystem::path& path) {
    return core::make_error(code, std::move(message), {}, {{"path", path.string()}});
}

[[nodiscard]] Result<nlohmann::json> read_json(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return core::make_failure<nlohmann::json>(
            project_error(core::ErrorCode::Io, "could not open JSON file", path));
    }
    try {
        nlohmann::json document;
        input >> document;
        return document;
    } catch (const nlohmann::json::exception& error) {
        return core::make_failure<nlohmann::json>(
            project_error(core::ErrorCode::Validation, error.what(), path));
    }
}

[[nodiscard]] Result<void> write_json(const std::filesystem::path& path,
                                      const nlohmann::json& document) {
    return platform::atomic_write_file(path, document.dump(2) + "\n");
}

[[nodiscard]] Result<void> reject_unknown_fields(const nlohmann::json& value,
                                                 std::initializer_list<std::string_view> allowed,
                                                 std::string_view context) {
    if (!value.is_object()) {
        return core::make_failure<void>(core::make_error(
            core::ErrorCode::Validation, "expected a JSON object", std::string(context)));
    }
    for (auto iterator = value.begin(); iterator != value.end(); ++iterator) {
        bool known = false;
        for (const std::string_view key : allowed) {
            if (iterator.key() == key) {
                known = true;
                break;
            }
        }
        if (!known) {
            return core::make_failure<void>(
                core::make_error(core::ErrorCode::Validation, "unknown field",
                                 "Remove the field or add it to the format.",
                                 {{"context", std::string(context)}, {"field", iterator.key()}}));
        }
    }
    return {};
}

[[nodiscard]] Result<core::Uuid> parse_uuid(const nlohmann::json& value, std::string_view field) {
    if (!value.is_string()) {
        return core::make_failure<core::Uuid>(core::make_error(core::ErrorCode::Validation,
                                                               "expected a UUID string", {},
                                                               {{"field", std::string(field)}}));
    }
    auto parsed = core::Uuid::parse(value.get<std::string>());
    if (!parsed) {
        return core::make_failure<core::Uuid>(parsed.error());
    }
    return *parsed;
}

[[nodiscard]] Result<std::string> require_string(const nlohmann::json& value,
                                                 std::string_view field) {
    const auto iterator = value.find(std::string(field));
    if (iterator == value.end() || !iterator->is_string()) {
        return core::make_failure<std::string>(core::make_error(
            core::ErrorCode::Validation, "expected a string", {}, {{"field", std::string(field)}}));
    }
    return iterator->get<std::string>();
}

[[nodiscard]] Result<int> require_int(const nlohmann::json& value, std::string_view field) {
    const auto iterator = value.find(std::string(field));
    if (iterator == value.end() || !iterator->is_number_integer()) {
        return core::make_failure<int>(core::make_error(core::ErrorCode::Validation,
                                                        "expected an integer", {},
                                                        {{"field", std::string(field)}}));
    }
    return iterator->get<int>();
}

[[nodiscard]] Result<std::filesystem::path> resolve_path(const std::filesystem::path& root,
                                                         std::string_view relative) {
    auto sandbox = core::PathSandbox::create(root);
    if (!sandbox) {
        return core::make_failure<std::filesystem::path>(sandbox.error());
    }
    return sandbox->resolve(relative);
}

[[nodiscard]] Result<ProjectSettings> parse_settings(const nlohmann::json& value) {
    auto unknown =
        reject_unknown_fields(value, {"defaultScene", "tickRate", "window"}, "project.settings");
    if (!unknown) {
        return core::make_failure<ProjectSettings>(unknown.error());
    }

    ProjectSettings settings;
    auto default_scene = require_string(value, "defaultScene");
    if (!default_scene) {
        return core::make_failure<ProjectSettings>(default_scene.error());
    }
    settings.defaultScene = std::move(*default_scene);

    auto tick_rate = require_int(value, "tickRate");
    if (!tick_rate || *tick_rate < 1 || *tick_rate > 1000) {
        return core::make_failure<ProjectSettings>(
            core::make_error(core::ErrorCode::Validation, "tickRate must be between 1 and 1000"));
    }
    settings.tickRate = *tick_rate;

    const auto window_iterator = value.find("window");
    if (window_iterator == value.end() || !window_iterator->is_object()) {
        return core::make_failure<ProjectSettings>(
            core::make_error(core::ErrorCode::Validation, "expected a window object"));
    }
    auto window_unknown = reject_unknown_fields(*window_iterator, {"width", "height", "mode"},
                                                "project.settings.window");
    if (!window_unknown) {
        return core::make_failure<ProjectSettings>(window_unknown.error());
    }
    auto width = require_int(*window_iterator, "width");
    auto height = require_int(*window_iterator, "height");
    auto mode = require_string(*window_iterator, "mode");
    if (!width || !height || !mode) {
        return core::make_failure<ProjectSettings>(core::make_error(
            core::ErrorCode::Validation, "window must contain width, height and mode"));
    }
    if (*width < 1 || *height < 1) {
        return core::make_failure<ProjectSettings>(
            core::make_error(core::ErrorCode::Validation, "window dimensions must be positive"));
    }
    if (*mode != "windowed" && *mode != "borderless" && *mode != "fullscreen") {
        return core::make_failure<ProjectSettings>(core::make_error(
            core::ErrorCode::Validation, "window.mode must be windowed, borderless or fullscreen"));
    }
    settings.window = WindowSettings{*width, *height, *mode};
    return settings;
}

} // namespace

struct Project::LockState {
    std::filesystem::path directory;

    explicit LockState(std::filesystem::path path) : directory(std::move(path)) {}

    ~LockState() {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};

Project::Project(std::filesystem::path root, core::Uuid project_id, std::string name,
                 std::string engine_version, std::vector<std::string> scenes,
                 std::string asset_root, std::string script_root, ProjectSettings settings,
                 nlohmann::json metadata)
    : root_(std::move(root)), project_id_(project_id), name_(std::move(name)),
      engine_version_(std::move(engine_version)), scenes_(std::move(scenes)),
      asset_root_(std::move(asset_root)), script_root_(std::move(script_root)),
      settings_(std::move(settings)), metadata_(std::move(metadata)) {}

Result<Project> Project::create(const std::filesystem::path& root, std::string name) {
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(root, error);
    if (error) {
        return core::make_failure<Project>(
            project_error(core::ErrorCode::Io, "could not resolve project root", root));
    }
    if (std::filesystem::exists(canonical / "project.json")) {
        return core::make_failure<Project>(core::make_error(core::ErrorCode::Conflict,
                                                            "project.json already exists", {},
                                                            {{"path", canonical.string()}}));
    }

    std::filesystem::create_directories(canonical / "scenes");
    std::filesystem::create_directories(canonical / "assets");
    std::filesystem::create_directories(canonical / "scripts");
    std::filesystem::create_directories(canonical / ".engine");

    ProjectSettings settings;
    Project project(canonical, core::Uuid::generate(), std::move(name), "0.1.0",
                    {"scenes/main.scene.json"}, "assets", "scripts", settings,
                    nlohmann::json::object());

    core::ReflectionRegistry registry;
    register_scene_components(registry);
    Scene scene;
    scene.set_scene_name("main");
    auto scene_saved = project.save_scene("scenes/main.scene.json", scene, registry);
    if (!scene_saved) {
        return core::make_failure<Project>(scene_saved.error());
    }
    auto saved = project.save();
    if (!saved) {
        return core::make_failure<Project>(saved.error());
    }
    return project;
}

Result<Project> Project::load(const std::filesystem::path& root) {
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(root, error);
    if (error) {
        return core::make_failure<Project>(
            project_error(core::ErrorCode::Io, "could not resolve project root", root));
    }
    auto document = read_json(canonical / "project.json");
    if (!document) {
        return core::make_failure<Project>(document.error());
    }
    auto unknown = reject_unknown_fields(*document,
                                         {
                                             "formatVersion",
                                             "projectId",
                                             "name",
                                             "engineVersion",
                                             "scenes",
                                             "assetRoot",
                                             "scriptRoot",
                                             "settings",
                                             "metadata",
                                         },
                                         "project");
    if (!unknown) {
        return core::make_failure<Project>(unknown.error());
    }

    auto format_version = require_int(*document, "formatVersion");
    if (!format_version || *format_version != kFormatVersion) {
        return core::make_failure<Project>(
            core::make_error(core::ErrorCode::Unsupported, "unsupported project format version",
                             "Only formatVersion 1 is supported.",
                             {{"formatVersion", format_version.value_or(-1)}}));
    }

    const auto id_iterator = document->find("projectId");
    if (id_iterator == document->end()) {
        return core::make_failure<Project>(
            core::make_error(core::ErrorCode::Validation, "projectId is missing"));
    }
    auto project_id = parse_uuid(*id_iterator, "projectId");
    if (!project_id) {
        return core::make_failure<Project>(project_id.error());
    }

    auto name = require_string(*document, "name");
    auto engine_version = require_string(*document, "engineVersion");
    auto asset_root = require_string(*document, "assetRoot");
    auto script_root = require_string(*document, "scriptRoot");
    if (!name || !engine_version || !asset_root || !script_root) {
        return core::make_failure<Project>(
            core::make_error(core::ErrorCode::Validation, "project fields are missing"));
    }

    const auto scenes_iterator = document->find("scenes");
    if (scenes_iterator == document->end() || !scenes_iterator->is_array()) {
        return core::make_failure<Project>(
            core::make_error(core::ErrorCode::Validation, "scenes must be an array"));
    }
    std::vector<std::string> scenes;
    for (const auto& scene : *scenes_iterator) {
        if (!scene.is_string()) {
            return core::make_failure<Project>(
                core::make_error(core::ErrorCode::Validation, "scene paths must be strings"));
        }
        scenes.push_back(scene.get<std::string>());
    }
    std::ranges::sort(scenes);

    const auto settings_iterator = document->find("settings");
    if (settings_iterator == document->end()) {
        return core::make_failure<Project>(
            core::make_error(core::ErrorCode::Validation, "settings is missing"));
    }
    auto settings = parse_settings(*settings_iterator);
    if (!settings) {
        return core::make_failure<Project>(settings.error());
    }

    nlohmann::json metadata = nlohmann::json::object();
    if (const auto metadata_iterator = document->find("metadata");
        metadata_iterator != document->end()) {
        if (!metadata_iterator->is_object()) {
            return core::make_failure<Project>(
                core::make_error(core::ErrorCode::Validation, "metadata must be an object"));
        }
        metadata = *metadata_iterator;
    }

    Project project(canonical, *project_id, std::move(*name), std::move(*engine_version),
                    std::move(scenes), std::move(*asset_root), std::move(*script_root),
                    std::move(*settings), std::move(metadata));
    return project;
}

Result<void> Project::save() const {
    nlohmann::json window = nlohmann::json::object();
    window["width"] = settings_.window.width;
    window["height"] = settings_.window.height;
    window["mode"] = settings_.window.mode;

    nlohmann::json settings = nlohmann::json::object();
    settings["defaultScene"] = settings_.defaultScene;
    settings["tickRate"] = settings_.tickRate;
    settings["window"] = std::move(window);

    nlohmann::json document{
        {"formatVersion", kFormatVersion},
        {"projectId", project_id_.to_string()},
        {"name", name_},
        {"engineVersion", engine_version_},
        {"scenes", scenes_},
        {"assetRoot", asset_root_},
        {"scriptRoot", script_root_},
        {"settings", std::move(settings)},
        {"metadata", metadata_},
    };
    return write_json(root_ / "project.json", document);
}

Result<nlohmann::json> Project::scene_json(const Scene& scene,
                                           const core::ReflectionRegistry& registry) const {
    const auto* transform = registry.find("engine.scene.Transform");
    const auto* camera = registry.find("engine.scene.Camera");
    const auto* motion = registry.find("engine.scene.Motion");
    if (transform == nullptr || camera == nullptr || motion == nullptr) {
        return core::make_failure<nlohmann::json>(core::make_error(
            core::ErrorCode::NotFound, "scene component descriptors are not registered"));
    }

    nlohmann::json entities = nlohmann::json::array();
    for (const Entity entity : scene.entities()) {
        const auto id = scene.uuid(entity);
        if (!id.has_value()) {
            continue;
        }

        nlohmann::json components = nlohmann::json::object();
        components["engine.scene.Transform"] = transform->to_json(&scene.transform(entity));
        if (scene.has<Camera>(entity)) {
            components["engine.scene.Camera"] = camera->to_json(&scene.component<Camera>(entity));
        }
        if (scene.has<Motion>(entity)) {
            components["engine.scene.Motion"] = motion->to_json(&scene.component<Motion>(entity));
        }

        nlohmann::json parent_id = nullptr;
        if (const Entity parent = scene.parent(entity); parent != entt::null) {
            if (const auto parent_uuid = scene.uuid(parent); parent_uuid.has_value()) {
                parent_id = parent_uuid->to_string();
            }
        }

        entities.push_back({
            {"id", id->to_string()},
            {"name", std::string(scene.name(entity))},
            {"parentId", std::move(parent_id)},
            {"components", std::move(components)},
            {"metadata", scene.metadata(entity)},
        });
    }

    return nlohmann::json{
        {"formatVersion", kFormatVersion}, {"sceneId", scene.scene_id().to_string()},
        {"name", scene.scene_name()},      {"revision", scene.revision()},
        {"entities", std::move(entities)}, {"metadata", scene.scene_metadata()},
    };
}

Result<void> Project::save_scene(const std::string& relative_path, const Scene& scene,
                                 const core::ReflectionRegistry& registry) const {
    auto path = resolve_path(root_, relative_path);
    if (!path) {
        return core::make_failure<void>(path.error());
    }
    auto document = scene_json(scene, registry);
    if (!document) {
        return core::make_failure<void>(document.error());
    }
    return write_json(*path, *document);
}

Result<Scene> Project::load_scene(const std::string& relative_path,
                                  const core::ReflectionRegistry& registry) const {
    auto path = resolve_path(root_, relative_path);
    if (!path) {
        return core::make_failure<Scene>(path.error());
    }
    auto document = read_json(*path);
    if (!document) {
        return core::make_failure<Scene>(document.error());
    }
    auto unknown = reject_unknown_fields(
        *document, {"formatVersion", "sceneId", "name", "revision", "entities", "metadata"},
        "scene");
    if (!unknown) {
        return core::make_failure<Scene>(unknown.error());
    }

    auto format_version = require_int(*document, "formatVersion");
    if (!format_version || *format_version != kFormatVersion) {
        return core::make_failure<Scene>(
            core::make_error(core::ErrorCode::Unsupported, "unsupported scene format version",
                             "Only formatVersion 1 is supported.",
                             {{"formatVersion", format_version.value_or(-1)}}));
    }

    auto scene_id = parse_uuid(document->at("sceneId"), "sceneId");
    auto name = require_string(*document, "name");
    auto revision = require_int(*document, "revision");
    if (!scene_id || !name || !revision) {
        return core::make_failure<Scene>(
            core::make_error(core::ErrorCode::Validation, "scene header is invalid"));
    }

    nlohmann::json metadata = nlohmann::json::object();
    if (const auto metadata_iterator = document->find("metadata");
        metadata_iterator != document->end()) {
        if (!metadata_iterator->is_object()) {
            return core::make_failure<Scene>(
                core::make_error(core::ErrorCode::Validation, "scene metadata must be an object"));
        }
        metadata = *metadata_iterator;
    }

    const auto entities_iterator = document->find("entities");
    if (entities_iterator == document->end() || !entities_iterator->is_array()) {
        return core::make_failure<Scene>(
            core::make_error(core::ErrorCode::Validation, "entities must be an array"));
    }

    Scene scene;
    scene.set_scene_id(*scene_id);
    scene.set_scene_name(std::move(*name));
    scene.set_scene_metadata(std::move(metadata));

    const auto* transform = registry.find("engine.scene.Transform");
    const auto* camera = registry.find("engine.scene.Camera");
    const auto* motion = registry.find("engine.scene.Motion");
    if (transform == nullptr || camera == nullptr || motion == nullptr) {
        return core::make_failure<Scene>(core::make_error(
            core::ErrorCode::NotFound, "scene component descriptors are not registered"));
    }

    for (const auto& entity_document : *entities_iterator) {
        auto entity_unknown = reject_unknown_fields(
            entity_document, {"id", "name", "parentId", "components", "metadata"}, "scene.entity");
        if (!entity_unknown) {
            return core::make_failure<Scene>(entity_unknown.error());
        }
        auto id = parse_uuid(entity_document.at("id"), "id");
        auto entity_name = require_string(entity_document, "name");
        if (!id || !entity_name) {
            return core::make_failure<Scene>(
                core::make_error(core::ErrorCode::Validation, "entity id or name is invalid"));
        }
        auto created = scene.create_entity(std::move(*entity_name), *id);
        if (!created) {
            return core::make_failure<Scene>(created.error());
        }
        if (const auto metadata_iterator = entity_document.find("metadata");
            metadata_iterator != entity_document.end()) {
            if (!metadata_iterator->is_object()) {
                return core::make_failure<Scene>(core::make_error(
                    core::ErrorCode::Validation, "entity metadata must be an object"));
            }
            auto metadata_set = scene.set_metadata(*created, *metadata_iterator);
            if (!metadata_set) {
                return core::make_failure<Scene>(metadata_set.error());
            }
        }
    }

    for (const auto& entity_document : *entities_iterator) {
        auto id = parse_uuid(entity_document.at("id"), "id");
        if (!id) {
            return core::make_failure<Scene>(id.error());
        }
        const auto entity = scene.find(*id);
        if (!entity.has_value()) {
            return core::make_failure<Scene>(
                core::make_error(core::ErrorCode::Validation, "entity disappeared during load"));
        }
        const Entity entity_handle = entity.value_or(entt::null);

        const auto parent_iterator = entity_document.find("parentId");
        if (parent_iterator != entity_document.end() && !parent_iterator->is_null()) {
            auto parent_id = parse_uuid(*parent_iterator, "parentId");
            if (!parent_id) {
                return core::make_failure<Scene>(parent_id.error());
            }
            const auto parent_entity = scene.find(*parent_id);
            if (!parent_entity.has_value()) {
                return core::make_failure<Scene>(
                    core::make_error(core::ErrorCode::Validation, "parent entity does not exist",
                                     {}, {{"parentId", parent_id->to_string()}}));
            }
            auto parent_set = scene.set_parent(entity_handle, parent_entity.value_or(entt::null));
            if (!parent_set) {
                return core::make_failure<Scene>(parent_set.error());
            }
        }

        const auto components_iterator = entity_document.find("components");
        if (components_iterator == entity_document.end() || !components_iterator->is_object()) {
            return core::make_failure<Scene>(
                core::make_error(core::ErrorCode::Validation, "components must be an object"));
        }
        const auto transform_iterator = components_iterator->find("engine.scene.Transform");
        if (transform_iterator == components_iterator->end()) {
            return core::make_failure<Scene>(
                core::make_error(core::ErrorCode::Validation, "Transform component is required"));
        }
        for (auto component_iterator = components_iterator->begin();
             component_iterator != components_iterator->end(); ++component_iterator) {
            if (component_iterator.key() == "engine.scene.Transform") {
                auto parsed =
                    transform->from_json(&scene.transform(entity_handle), *component_iterator);
                if (!parsed) {
                    return core::make_failure<Scene>(parsed.error());
                }
            } else if (component_iterator.key() == "engine.scene.Camera") {
                auto added = scene.add_component<Camera>(entity_handle);
                if (!added) {
                    return core::make_failure<Scene>(added.error());
                }
                auto parsed =
                    camera->from_json(&scene.component<Camera>(entity_handle), *component_iterator);
                if (!parsed) {
                    return core::make_failure<Scene>(parsed.error());
                }
            } else if (component_iterator.key() == "engine.scene.Motion") {
                auto added = scene.add_component<Motion>(entity_handle);
                if (!added) {
                    return core::make_failure<Scene>(added.error());
                }
                auto parsed =
                    motion->from_json(&scene.component<Motion>(entity_handle), *component_iterator);
                if (!parsed) {
                    return core::make_failure<Scene>(parsed.error());
                }
            } else {
                return core::make_failure<Scene>(
                    core::make_error(core::ErrorCode::Validation, "unknown component type", {},
                                     {{"component", component_iterator.key()}}));
            }
        }
    }

    scene.set_revision(static_cast<std::uint64_t>(*revision));
    return scene;
}

Result<void> Project::add_scene(std::string relative_path) {
    auto path = resolve_path(root_, relative_path);
    if (!path) {
        return core::make_failure<void>(path.error());
    }
    if (std::ranges::find(scenes_, relative_path) != scenes_.end()) {
        return core::make_failure<void>(core::make_error(core::ErrorCode::Conflict,
                                                         "scene is already registered", {},
                                                         {{"path", relative_path}}));
    }
    scenes_.push_back(std::move(relative_path));
    std::ranges::sort(scenes_);
    return save();
}

Result<void> Project::force_unlock(const std::filesystem::path& root) {
    std::error_code error;
    const std::filesystem::path directory = root / ".engine" / "lock";
    if (!std::filesystem::exists(directory, error)) {
        return {};
    }
    std::filesystem::remove_all(directory, error);
    if (error) {
        return core::make_failure<void>(
            project_error(core::ErrorCode::Io, "could not remove project lock", directory));
    }
    return {};
}

Result<void> Project::lock() {
    if (lock_ != nullptr) {
        return core::make_failure<void>(
            core::make_error(core::ErrorCode::Conflict, "project is already locked"));
    }
    const std::filesystem::path directory = root_ / ".engine" / "lock";
    std::error_code error;
    if (!std::filesystem::create_directory(directory, error)) {
        return core::make_failure<void>(
            project_error(core::ErrorCode::Conflict, "project lock already exists", directory));
    }

    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
#ifdef _WIN32
    const int pid = _getpid();
#else
    const int pid = ::getpid();
#endif
    const nlohmann::json lock_document{{"pid", pid}, {"startedAt", now}};
    auto written = write_json(directory / "lock.json", lock_document);
    if (!written) {
        std::filesystem::remove_all(directory, error);
        return core::make_failure<void>(written.error());
    }

    lock_ = std::make_shared<LockState>(directory);
    return {};
}

void Project::unlock() noexcept {
    lock_.reset();
}

bool Project::locked() const noexcept {
    return lock_ != nullptr;
}

const std::filesystem::path& Project::root() const noexcept {
    return root_;
}

const core::Uuid& Project::project_id() const noexcept {
    return project_id_;
}

const std::string& Project::name() const noexcept {
    return name_;
}

const std::vector<std::string>& Project::scenes() const noexcept {
    return scenes_;
}

const ProjectSettings& Project::settings() const noexcept {
    return settings_;
}

const nlohmann::json& Project::metadata() const noexcept {
    return metadata_;
}

} // namespace engine::scene
