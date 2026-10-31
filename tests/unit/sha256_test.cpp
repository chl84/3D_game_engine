#include <engine/core/sha256.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("SHA-256 matches the standard test vectors", "[core][sha256]") {
    CHECK(engine::core::sha256_hex("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(engine::core::sha256_hex("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(engine::core::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("SHA-256 handles block boundaries and long input", "[core][sha256]") {
    const std::string fifty_five(55, 'a');
    const std::string fifty_six(56, 'a');
    const std::string sixty_four(64, 'a');
    CHECK(engine::core::sha256_hex(fifty_five) ==
          "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    CHECK(engine::core::sha256_hex(fifty_six) ==
          "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    CHECK(engine::core::sha256_hex(sixty_four) ==
          "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    const std::string million(1000000, 'a');
    CHECK(engine::core::sha256_hex(million) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
