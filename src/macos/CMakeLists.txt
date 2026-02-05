# Konflikt macOS Application
#
# NOTE: CMake's Swift/Ninja integration uses a combined compile+link rule,
# which causes Swift files to be recompiled whenever link dependencies change.
# This is a known CMake limitation. For faster incremental builds during
# development, use the Xcode generator instead:
#   cmake .. -G Xcode -DBUILD_UI=OFF
#   open Konflikt.xcodeproj

# Enable Swift language support
set(CMAKE_Swift_LANGUAGE_VERSION 5)

# Build the Objective-C++ bridge as a static library
add_library(KonfliktBridge STATIC
    KonfliktBridge/KonfliktBridge.mm
)

target_include_directories(KonfliktBridge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/KonfliktBridge
)

target_link_libraries(KonfliktBridge
    PUBLIC
        konflikt
)

# Objective-C++ specific flags (enable ARC)
set_target_properties(KonfliktBridge PROPERTIES
    COMPILE_FLAGS "-x objective-c++ -fobjc-arc"
)

# Find required frameworks
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(APPKIT_FRAMEWORK AppKit REQUIRED)

# Configure Info.plist (must be done before setting target properties)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    @ONLY
)

# Swift application
add_executable(KonfliktApp MACOSX_BUNDLE
    Konflikt/main.swift
    Konflikt/AppDelegate.swift
    Konflikt/StatusBarController.swift
    Konflikt/PreferencesWindowController.swift
)

target_link_libraries(KonfliktApp
    PRIVATE
        KonfliktBridge
        ${COCOA_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
)

# Set Swift-specific properties
set_target_properties(KonfliktApp PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.konflikt.app"
    MACOSX_BUNDLE_BUNDLE_NAME "Konflikt"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Konflikt/Konflikt-Bridging-Header.h"
    OUTPUT_NAME "Konflikt"
)

# Set bridging header for Swift (works with both Ninja and Xcode)
target_compile_options(KonfliktApp PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-import-objc-header;${CMAKE_CURRENT_SOURCE_DIR}/Konflikt/Konflikt-Bridging-Header.h>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I;${CMAKE_CURRENT_SOURCE_DIR}/KonfliktBridge>"
)
