cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

project(konflikt_native)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include Node-API headers
include_directories(${CMAKE_JS_INC})

# Include native headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCE_FILES
    src/konflikt_native.cpp
)

# Platform-specific sources and libraries
if(APPLE)
    list(APPEND SOURCE_FILES src/platform_macos.cpp)
    find_library(APPLICATION_SERVICES ApplicationServices)
    find_library(CARBON Carbon)
    set(PLATFORM_LIBS ${APPLICATION_SERVICES} ${CARBON})
elseif(UNIX)
    list(APPEND SOURCE_FILES src/platform_x11.cpp)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB REQUIRED xcb xcb-xinput xcb-xtest xcb-xkb xkbcommon xkbcommon-x11)
    set(PLATFORM_LIBS ${XCB_LIBRARIES})
    include_directories(${XCB_INCLUDE_DIRS})
    link_directories(${XCB_LIBRARY_DIRS})
endif()

# Create the native addon
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})

# Set library properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Link libraries
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB} ${PLATFORM_LIBS})

# Include Node-API
execute_process(
    COMMAND node -p "require('node-addon-api').include"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
)
string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${NODE_ADDON_API_DIR})

# Define NAPI_VERSION
add_definitions(-DNAPI_VERSION=8)

# Platform-specific compile options
if(APPLE)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
elseif(UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
    # Link pthread for threading support
    target_link_libraries(${PROJECT_NAME} pthread)
endif()

# Build output directory based on build type
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../dist/build/${CMAKE_BUILD_TYPE}
)
