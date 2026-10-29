option(ENGINE_SANITIZE_ADDRESS "Build with AddressSanitizer" OFF)
option(ENGINE_SANITIZE_UNDEFINED "Build with UndefinedBehaviorSanitizer" OFF)
option(ENGINE_SANITIZE_THREAD "Build with ThreadSanitizer" OFF)

if(ENGINE_SANITIZE_THREAD AND (ENGINE_SANITIZE_ADDRESS OR ENGINE_SANITIZE_UNDEFINED))
    message(FATAL_ERROR "ThreadSanitizer cannot be combined with Address/UndefinedBehaviorSanitizer")
endif()

if(MSVC AND (ENGINE_SANITIZE_ADDRESS OR ENGINE_SANITIZE_UNDEFINED OR ENGINE_SANITIZE_THREAD))
    message(FATAL_ERROR "Sanitizer presets are supported on GCC and Clang only")
endif()

if(ENGINE_SANITIZE_ADDRESS OR ENGINE_SANITIZE_UNDEFINED)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(ENGINE_SANITIZE_THREAD)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()
