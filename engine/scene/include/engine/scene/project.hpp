#pragma once

#include <engine/core/reflect.hpp>
#include <engine/core/uuid.hpp>
#include <engine/scene/scene.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace engine::scene {

struct WindowSettings {
    int width{1920};
    int height{1080};
    std::string mode{"windowed"};
};

struct ProjectSettings {
    std::string defaultScene{"scenes/main.scene.json"};
    int tickRate{60};
    WindowSettings window;
};

class Project {
public:
    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;
    Project(Project&&) noexcept = default;
    Project& operator=(Project&&) noexcept = default;
    ~Project() = default;

    [[nodiscard]] static Result<Project> create(const std::filesystem::path& root,
                                                std::string name);
    [[nodiscard]] static Result<Project> load(const std::filesystem::path& root);

    [[nodiscard]] Result<void> save() const;
    [[nodiscard]] Result<void> save_scene(const std::string& relative_path, const Scene& scene,
                                          const core::ReflectionRegistry& registry) const;
    [[nodiscard]] Result<Scene> load_scene(const std::string& relative_path,
                                           const core::ReflectionRegistry& registry) const;

    [[nodiscard]] Result<void> lock();
    void unlock() noexcept;
    [[nodiscard]] bool locked() const noexcept;

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] const core::Uuid& project_id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] const std::vector<std::string>& scenes() const noexcept;
    [[nodiscard]] const ProjectSettings& settings() const noexcept;
    [[nodiscard]] const nlohmann::json& metadata() const noexcept;

private:
    struct LockState;

    Project(std::filesystem::path root, core::Uuid project_id, std::string name,
            std::string engine_version, std::vector<std::string> scenes, std::string asset_root,
            std::string script_root, ProjectSettings settings, nlohmann::json metadata);

    std::filesystem::path root_;
    core::Uuid project_id_;
    std::string name_;
    std::string engine_version_{"0.1.0"};
    std::vector<std::string> scenes_;
    std::string asset_root_{"assets"};
    std::string script_root_{"scripts"};
    ProjectSettings settings_;
    nlohmann::json metadata_ = nlohmann::json::object();
    std::shared_ptr<LockState> lock_;
};

} // namespace engine::scene
