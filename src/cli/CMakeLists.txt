find_package(CLI11 CONFIG REQUIRED)

add_executable(anchor-cli
    main.cpp
    Application.cpp
    Client.cpp
    DiagnosticsCommand.cpp
    SyncCommand.cpp
    BackupCommands.cpp
)
target_link_libraries(anchor-cli PRIVATE anchor CLI11::CLI11)
set_target_properties(anchor-cli PROPERTIES OUTPUT_NAME anchor)

