cmake_minimum_required(VERSION 3.21)
project(Konflikt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)

# Options
option(BUILD_MACOS_APP "Build macOS application" OFF)
option(BUILD_LINUX_APP "Build Linux application" ON)
option(BUILD_UI "Build React UI" ON)
option(BUILD_TESTS "Build tests" OFF)

# Platform detection
if(APPLE)
    set(BUILD_MACOS_APP ON)
    set(BUILD_LINUX_APP OFF)
elseif(UNIX)
    set(BUILD_LINUX_APP ON)
    set(BUILD_MACOS_APP OFF)
endif()

# ============================================================================
# Third-party dependencies
# ============================================================================

# Glaze (header-only JSON library)
add_library(glaze INTERFACE)
target_include_directories(glaze INTERFACE ${CMAKE_SOURCE_DIR}/third_party/glaze/include)

# uSockets
set(USOCKETS_DIR ${CMAKE_SOURCE_DIR}/third_party/uSockets)
file(GLOB USOCKETS_SOURCES
    ${USOCKETS_DIR}/src/*.c
    ${USOCKETS_DIR}/src/eventing/*.c
    ${USOCKETS_DIR}/src/crypto/*.c
)

add_library(uSockets STATIC ${USOCKETS_SOURCES})
target_include_directories(uSockets PUBLIC ${USOCKETS_DIR}/src)
target_compile_definitions(uSockets PRIVATE LIBUS_NO_SSL)

if(APPLE)
    target_compile_definitions(uSockets PRIVATE LIBUS_USE_KQUEUE)
else()
    target_compile_definitions(uSockets PRIVATE LIBUS_USE_EPOLL)
endif()

# uWebSockets (header-only, depends on uSockets)
add_library(uWebSockets INTERFACE)
target_include_directories(uWebSockets INTERFACE ${CMAKE_SOURCE_DIR}/third_party/uWebSockets/src)
target_link_libraries(uWebSockets INTERFACE uSockets)

# zlib for compression (uWebSockets needs it)
find_package(ZLIB REQUIRED)
target_link_libraries(uSockets PRIVATE ZLIB::ZLIB)

# ============================================================================
# libkonflikt - Core shared library
# ============================================================================
add_subdirectory(src/libkonflikt)

# ============================================================================
# Platform-specific applications
# ============================================================================

if(BUILD_LINUX_APP AND NOT APPLE)
    message(STATUS "Building Linux application")
    add_subdirectory(src/app)
endif()

if(BUILD_MACOS_APP AND APPLE)
    message(STATUS "Building macOS application")
    enable_language(Swift)
    add_subdirectory(src/macos)
endif()

# ============================================================================
# React UI build
# ============================================================================

if(BUILD_UI)
    # Check if npm is available
    find_program(NPM_EXECUTABLE npm)
    if(NPM_EXECUTABLE)
        add_custom_target(ui ALL
            COMMAND ${NPM_EXECUTABLE} run build
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/webpage
            COMMENT "Building React UI"
            VERBATIM
        )
    else()
        message(WARNING "npm not found - skipping UI build")
    endif()
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "Konflikt build configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Linux app: ${BUILD_LINUX_APP}")
message(STATUS "  Build macOS app: ${BUILD_MACOS_APP}")
message(STATUS "  Build React UI: ${BUILD_UI}")
message(STATUS "")
