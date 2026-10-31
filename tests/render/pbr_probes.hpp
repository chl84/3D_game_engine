#pragma once
#include <engine/render/device.hpp>
#include <vector>
namespace engine::test {
std::vector<std::uint8_t> render_pbr_color_chart(const render::Device& device);
void verify_pbr_probes(const render::Device& device);
} // namespace engine::test
