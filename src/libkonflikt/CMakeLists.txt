# libkonflikt - Core shared library for Konflikt KVM switch

set(LIBKONFLIKT_SOURCES
    src/Konflikt.cpp
    src/Protocol.cpp
    src/WebSocketServer.cpp
    src/WebSocketClient.cpp
    src/HttpServer.cpp
    src/LayoutManager.cpp
    src/Rect.cpp
)

# Platform-specific sources
if(APPLE)
    list(APPEND LIBKONFLIKT_SOURCES
        src/PlatformMac.mm
        src/ServiceDiscoveryMac.mm
    )
elseif(UNIX)
    list(APPEND LIBKONFLIKT_SOURCES
        src/PlatformLinux.cpp
        src/ServiceDiscoveryLinux.cpp
    )
endif()

add_library(konflikt STATIC ${LIBKONFLIKT_SOURCES})

target_include_directories(konflikt
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Find OpenSSL for SHA256
find_package(OpenSSL REQUIRED)

target_link_libraries(konflikt
    PUBLIC
        glaze
        uWebSockets
        OpenSSL::Crypto
)

# Platform-specific libraries
if(APPLE)
    find_library(APPLICATION_SERVICES ApplicationServices REQUIRED)
    find_library(CARBON Carbon REQUIRED)
    find_library(COCOA Cocoa REQUIRED)
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
    target_link_libraries(konflikt
        PUBLIC
            ${APPLICATION_SERVICES}
            ${CARBON}
            ${COCOA}
            ${CORE_FOUNDATION}
    )
elseif(UNIX)
    find_package(PkgConfig REQUIRED)

    # X11/XCB dependencies
    pkg_check_modules(XCB REQUIRED
        xcb
        xcb-xinput
        xcb-xtest
        xcb-xkb
        xcb-randr
        xkbcommon
        xkbcommon-x11
    )

    target_include_directories(konflikt PRIVATE ${XCB_INCLUDE_DIRS})
    target_link_libraries(konflikt PUBLIC ${XCB_LIBRARIES} pthread)
    target_link_directories(konflikt PUBLIC ${XCB_LIBRARY_DIRS})

    # Avahi for mDNS (optional for now)
    pkg_check_modules(AVAHI avahi-client)
    if(AVAHI_FOUND)
        target_include_directories(konflikt PRIVATE ${AVAHI_INCLUDE_DIRS})
        target_link_libraries(konflikt PUBLIC ${AVAHI_LIBRARIES})
        target_compile_definitions(konflikt PRIVATE HAVE_AVAHI)
    endif()
endif()

# Compiler options
target_compile_options(konflikt PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(konflikt PRIVATE DEBUG)
endif()
