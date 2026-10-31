#include <engine/core/sha256.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace engine::core {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
        0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
        0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
        0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
        0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
        0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
        0xc67178f2U,
    },
};

[[nodiscard]] constexpr std::uint32_t rotate_right(std::uint32_t value,
                                                   std::uint32_t bits) noexcept {
    return (value >> bits) | (value << (32U - bits));
}

void compress(std::array<std::uint32_t, 8>& state,
              const std::array<std::uint8_t, 64>& block) noexcept {
    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t index = 0; index < 16; ++index) {
        const std::size_t offset = index * 4U;
        schedule[index] = (static_cast<std::uint32_t>(block[offset]) << 24U) |
                          (static_cast<std::uint32_t>(block[offset + 1]) << 16U) |
                          (static_cast<std::uint32_t>(block[offset + 2]) << 8U) |
                          static_cast<std::uint32_t>(block[offset + 3]);
    }
    for (std::size_t index = 16; index < 64; ++index) {
        const std::uint32_t s0 = rotate_right(schedule[index - 15], 7U) ^
                                 rotate_right(schedule[index - 15], 18U) ^
                                 (schedule[index - 15] >> 3U);
        const std::uint32_t s1 = rotate_right(schedule[index - 2], 17U) ^
                                 rotate_right(schedule[index - 2], 19U) ^
                                 (schedule[index - 2] >> 10U);
        schedule[index] = schedule[index - 16] + s0 + schedule[index - 7] + s1;
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];
    for (std::size_t index = 0; index < 64; ++index) {
        const std::uint32_t s1 = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^ rotate_right(e, 25U);
        const std::uint32_t choose = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + s1 + choose + kRoundConstants[index] + schedule[index];
        const std::uint32_t s0 = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^ rotate_right(a, 22U);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

} // namespace

std::string sha256_hex(std::span<const std::byte> bytes) {
    std::array<std::uint32_t, 8> state{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    };

    std::size_t offset = 0;
    while (bytes.size() - offset >= 64U) {
        std::array<std::uint8_t, 64> block{};
        std::memcpy(block.data(), bytes.data() + offset, block.size());
        compress(state, block);
        offset += block.size();
    }

    std::array<std::uint8_t, 128> tail{};
    const std::size_t remainder = bytes.size() - offset;
    if (remainder > 0U) {
        std::memcpy(tail.data(), bytes.data() + offset, remainder);
    }
    tail[remainder] = 0x80U;
    const std::size_t tail_size = remainder < 56U ? 64U : 128U;
    const std::uint64_t bit_length = static_cast<std::uint64_t>(bytes.size()) * 8U;
    for (std::size_t index = 0; index < 8; ++index) {
        tail[tail_size - 1U - index] =
            static_cast<std::uint8_t>((bit_length >> (index * 8U)) & 0xFFU);
    }
    for (std::size_t index = 0; index < tail_size; index += 64U) {
        std::array<std::uint8_t, 64> block{};
        std::memcpy(block.data(), tail.data() + index, block.size());
        compress(state, block);
    }

    std::string hex;
    hex.reserve(64);
    constexpr char kDigits[] = "0123456789abcdef";
    for (const std::uint32_t word : state) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            hex.push_back(kDigits[(word >> static_cast<std::uint32_t>(shift)) & 0x0FU]);
        }
    }
    return hex;
}

std::string sha256_hex(std::string_view text) {
    return sha256_hex(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(text.data()), text.size()));
}

} // namespace engine::core
