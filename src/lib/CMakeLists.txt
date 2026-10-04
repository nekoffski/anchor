
add_library(anchor STATIC
    # -- base
    "base/Core.cpp"
    "base/Error.cpp"
    "base/FileSystem.cpp"
    "base/Log.cpp"
    "base/Profiler.cpp"
    "base/Random.cpp"
    "base/String.cpp"
    "base/Id.cpp"
    "base/Config.cpp"
    "base/Handle.cpp"
    "base/Json.cpp"
    "base/runtime/Thread.cpp"
    "base/runtime/SignalHandler.cpp"
    # -- proto
    "proto/Message.cpp"
    # -- adapters
    "adapters/asio/AsioServer.cpp"
    "adapters/asio/AsioSession.cpp"
    "adapters/asio/AsioController.cpp"
)

target_include_directories(anchor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(spdlog CONFIG REQUIRED)
find_package(stduuid CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)

target_link_libraries(anchor
    PUBLIC
        fmt::fmt
        spdlog::spdlog
        stduuid::stduuid
        nlohmann_json::nlohmann_json
        asio::asio
)

if (ANCHOR_ENABLE_PROFILING)
    target_compile_definitions(anchor PRIVATE ANCHOR_ENABLE_PROFILING=1)
endif()

# if (BUILD_TESTING)
    # add_subdirectory(tests)
# endif()
