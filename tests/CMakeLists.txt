find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(
    NAME engine_version_json
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/smoke/version_json.py"
            "$<TARGET_FILE:engine>"
)

set_tests_properties(engine_version_json PROPERTIES
    LABELS "unit;cli"
)
