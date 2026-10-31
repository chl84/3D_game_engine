#include "admission.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/ConfigurationString.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#if !defined(JPH_DOUBLE_PRECISION) || !defined(JPH_CROSS_PLATFORM_DETERMINISTIC)
#error M4 admission requires the approved double-position deterministic Jolt profile
#endif
#if defined(JPH_USE_AVX) || defined(JPH_USE_AVX2) || defined(JPH_USE_AVX512) ||                    \
    defined(JPH_USE_FMADD) || defined(JPH_USE_F16C) || defined(JPH_USE_LZCNT) ||                   \
    defined(JPH_USE_TZCNT)
#error Unexpected CPU feature in the approved common Jolt profile
#endif

namespace admission {
namespace {

class JoltLifetime {
public:
    JoltLifetime() {
        JPH::RegisterDefaultAllocator();
        factory_ = std::make_unique<JPH::Factory>();
        require(JPH::Factory::sInstance == nullptr, "unexpected existing Jolt factory");
        JPH::Factory::sInstance = factory_.get();
        JPH::RegisterTypes();
    }
    ~JoltLifetime() {
        JPH::UnregisterTypes();
        JPH::Factory::sInstance = nullptr;
    }
    JoltLifetime(const JoltLifetime&) = delete;
    JoltLifetime& operator=(const JoltLifetime&) = delete;

private:
    std::unique_ptr<JPH::Factory> factory_;
};

class BroadLayers final : public JPH::BroadPhaseLayerInterface {
public:
    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override {
        return 2;
    }
    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return JPH::BroadPhaseLayer(static_cast<JPH::uint8>(layer));
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override {
        return "admission";
    }
#endif
};

class BroadFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer object,
                                     JPH::BroadPhaseLayer broad) const override {
        return object == 1 || broad.GetValue() == 1;
    }
};

class PairFilter final : public JPH::ObjectLayerPairFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return a == 1 || b == 1;
    }
};

std::vector<std::uint64_t> simulate() {
    BroadLayers layers;
    BroadFilter broad_filter;
    PairFilter pair_filter;
    JPH::PhysicsSystem world;
    world.Init(4096, 0, 16384, 16384, layers, broad_filter, pair_filter);
    JPH::PhysicsSettings settings;
    settings.mDeterministicSimulation = true;
    settings.mNumVelocitySteps = 10;
    settings.mNumPositionSteps = 2;
    world.SetPhysicsSettings(settings);
    world.SetGravity(JPH::Vec3(0.0F, -9.81F, 0.0F));
    JPH::TempAllocatorImpl temporary(64U * 1024U * 1024U);
    JPH::JobSystemSingleThreaded jobs(2048);
    auto floor_shape = JPH::BoxShapeSettings(JPH::Vec3(10.0F, 0.5F, 10.0F)).Create();
    auto box_shape = JPH::BoxShapeSettings(JPH::Vec3::sReplicate(0.5F)).Create();
    require(!floor_shape.HasError() && !box_shape.HasError(), "Jolt shape creation failed");
    auto& bodies = world.GetBodyInterface();
    const auto floor = bodies.CreateAndAddBody(
        JPH::BodyCreationSettings(floor_shape.Get(), JPH::RVec3(0.0, -0.5, 0.0),
                                  JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0),
        JPH::EActivation::DontActivate);
    const auto box = bodies.CreateAndAddBody(
        JPH::BodyCreationSettings(box_shape.Get(), JPH::RVec3(0.0, 3.0, 0.0),
                                  JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, 1),
        JPH::EActivation::Activate);
    require(!floor.IsInvalid() && !box.IsInvalid(), "Jolt body creation failed");
    world.OptimizeBroadPhase();
    std::vector<std::uint64_t> trace;
    trace.reserve(240);
    for (int tick = 0; tick < 240; ++tick) {
        require(world.Update(1.0F / 60.0F, 1, &temporary, &jobs) == JPH::EPhysicsUpdateError::None,
                "Jolt update capacity error");
        const double height = bodies.GetPosition(box).GetY();
        require(std::isfinite(height), "Jolt produced a non-finite position");
        trace.push_back(std::bit_cast<std::uint64_t>(height));
    }
    require(std::abs(bodies.GetPosition(box).GetY() - 0.5) < 0.03,
            "box did not settle on the floor");
    bodies.RemoveBody(box);
    bodies.RemoveBody(floor);
    bodies.DestroyBody(box);
    bodies.DestroyBody(floor);
    return trace;
}

} // namespace

nlohmann::json verify_jolt() {
    JoltLifetime lifetime;
    const auto first = simulate();
    const auto second = simulate();
    require(first == second, "Jolt repeat did not match at every sampled tick");
    return {{"sourceVersion", "5.6.0"},
            {"sourceCommit", "e77f175595e64cb44218cc9d9d56fc365ad0e36a"},
            {"configuration", JPH::GetConfigurationString()},
            {"configurationScope", "consumer headers; verify library flags separately"},
            {"jobThreads", 1},
            {"ticks", first.size()},
            {"positionTraceBits", first},
            {"repeatExact", true}};
}

} // namespace admission
