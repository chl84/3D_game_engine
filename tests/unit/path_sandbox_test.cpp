#include <catch2/catch_test_macros.hpp>

#include <engine/core/path_sandbox.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "temp_dir.hpp"

using engine::core::ErrorCode;
using engine::core::PathSandbox;

TEST_CASE("path sandbox resolves relative paths", "[core][path]") {
    engine::test::TempDir temp("sandbox");
    const auto sandbox = PathSandbox::create(temp.path());
    REQUIRE(sandbox.has_value());

    const auto resolved = sandbox->resolve("nested/file.txt");
    REQUIRE(resolved.has_value());
    REQUIRE(resolved->filename() == "file.txt");
    REQUIRE(resolved->parent_path().filename() == "nested");

    const auto root = sandbox->resolve(".");
    REQUIRE(root.has_value());
    REQUIRE(*root == sandbox->root());
}

TEST_CASE("path sandbox rejects escapes", "[core][path]") {
    engine::test::TempDir temp("sandbox_escape");
    const auto sandbox = PathSandbox::create(temp.path());
    REQUIRE(sandbox.has_value());

    REQUIRE_FALSE(sandbox->resolve("../escape"));
    REQUIRE_FALSE(sandbox->resolve("a/../../escape"));
    REQUIRE_FALSE(sandbox->resolve("/absolute"));
    REQUIRE_FALSE(sandbox->resolve(""));
}

TEST_CASE("path sandbox rejects symlink escapes", "[core][path]") {
    engine::test::TempDir root("sandbox_root");
    engine::test::TempDir outside("sandbox_outside");

    {
        std::ofstream output(outside.path() / "secret.txt");
        output << "secret";
    }

    std::error_code error;
    std::filesystem::create_symlink(outside.path(), root.path() / "link", error);
    if (error) {
        SKIP("symlinks are not available on this platform: " + error.message());
    }

    const auto sandbox = PathSandbox::create(root.path());
    REQUIRE(sandbox.has_value());

    const auto resolved = sandbox->resolve("link/secret.txt");
    REQUIRE_FALSE(resolved.has_value());
    REQUIRE(resolved.error().code == ErrorCode::Validation);
}

TEST_CASE("path sandbox requires an existing directory", "[core][path]") {
    engine::test::TempDir temp("sandbox_missing");

    const auto missing = PathSandbox::create(temp.path() / "missing");
    REQUIRE_FALSE(missing.has_value());
    REQUIRE(missing.error().code == ErrorCode::Io);

    const auto file_path = temp.path() / "file.txt";
    {
        std::ofstream output(file_path);
        output << "data";
    }

    const auto file = PathSandbox::create(file_path);
    REQUIRE_FALSE(file.has_value());
    REQUIRE(file.error().code == ErrorCode::Validation);
}
