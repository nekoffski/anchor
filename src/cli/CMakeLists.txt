find_package(CLI11 CONFIG REQUIRED)

add_library(anchor-cli-commands STATIC
    Application.cpp
    Client.cpp
    CommandContext.cpp
    DiagnosticsCommand.cpp
    BackupCommands.cpp
)
target_include_directories(anchor-cli-commands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(anchor-cli-commands PUBLIC anchor CLI11::CLI11)

add_executable(anchor-cli main.cpp)
target_link_libraries(anchor-cli PRIVATE anchor-cli-commands)
set_target_properties(anchor-cli PROPERTIES OUTPUT_NAME anchor)

