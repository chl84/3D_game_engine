find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(tl-expected CONFIG REQUIRED)

if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
endif()
