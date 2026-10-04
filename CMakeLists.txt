cmake_minimum_required(VERSION 3.6)
if(POLICY CMP0156)
    cmake_policy(SET CMP0156 NEW)
endif()
project(anchor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ANCHOR_ENABLE_PROFILING OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(ANCHOR_MIN_CXX_COMPILER_VERSION 14)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(ANCHOR_MIN_CXX_COMPILER_VERSION 18)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(ANCHOR_MIN_CXX_COMPILER_VERSION 17)
else()
    message(FATAL_ERROR
        "Unsupported C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}. "
        "Use GCC 14+, Clang 18+, or Apple Clang 17+ (Xcode 16.3+).")
endif()

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ANCHOR_MIN_CXX_COMPILER_VERSION)
    message(FATAL_ERROR
        "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} is too old. "
        "This project requires GCC 14+, Clang 18+, or Apple Clang 17+ (Xcode 16.3+).")
endif()

include(CTest)
add_subdirectory(src/lib)
add_subdirectory(src/daemon)
add_subdirectory(src/cli)

include(GNUInstallDirs)
install(TARGETS anchor-cli anchord
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES packaging/server.conf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/anchor
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ANCHOR_SYSTEMD_UNIT_DIR "lib/systemd/system" CACHE STRING
        "systemd unit install directory, relative to the install prefix")
    configure_file(packaging/anchord.service.in anchord.service @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/anchord.service"
        DESTINATION ${ANCHOR_SYSTEMD_UNIT_DIR}
    )
endif()
