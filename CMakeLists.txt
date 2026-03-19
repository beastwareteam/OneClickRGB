cmake_minimum_required(VERSION 3.16)
project(OneClickRGB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_GUI "Build Qt GUI application" ON)
option(BUILD_CLI "Build command-line interface" ON)
option(USE_OPENRGB_CONTROLLERS "Use OpenRGB controller implementations" OFF)

# Find dependencies
find_package(hidapi QUIET)
find_package(nlohmann_json QUIET)

# If not found, we'll use bundled versions
if(NOT nlohmann_json_FOUND)
    message(STATUS "Using bundled nlohmann_json")
    # Add as header-only
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/json/include)
endif()

# Source files
set(CORE_SOURCES
    src/core/DeviceManager.cpp
    src/core/ProfileManager.cpp
    src/core/OneClickRGB.cpp
    src/core/AutoStart.cpp
    src/core/ConfigManager.cpp
)

set(SCANNER_SOURCES
    src/scanner/HardwareScanner.cpp
)

set(DEVICE_SOURCES
    src/devices/RGBDevice.cpp
    src/devices/HIDController.cpp
)

# Core library (shared between CLI and GUI)
add_library(oneclickrgb_core STATIC
    ${CORE_SOURCES}
    ${SCANNER_SOURCES}
    ${DEVICE_SOURCES}
)

target_include_directories(oneclickrgb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
if(hidapi_FOUND)
    target_link_libraries(oneclickrgb_core PUBLIC hidapi::hidapi)
else()
    # Manual HIDAPI linking
    if(WIN32)
        target_link_libraries(oneclickrgb_core PUBLIC setupapi)
    endif()
endif()

if(nlohmann_json_FOUND)
    target_link_libraries(oneclickrgb_core PUBLIC nlohmann_json::nlohmann_json)
else()
    target_link_libraries(oneclickrgb_core PUBLIC nlohmann_json)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(oneclickrgb_core PUBLIC shell32)
endif()

# CLI Application
if(BUILD_CLI)
    add_executable(oneclickrgb src/main.cpp)
    target_link_libraries(oneclickrgb PRIVATE oneclickrgb_core)

    install(TARGETS oneclickrgb DESTINATION bin)
endif()

# GUI Application (Qt)
if(BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt5 COMPONENTS Widgets QUIET)
    endif()

    if(Qt6_FOUND OR Qt5_FOUND)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTOUIC ON)
        set(CMAKE_AUTORCC ON)

        set(GUI_SOURCES
            src/ui/MainWindow.cpp
            src/ui/DeviceCard.cpp
            src/ui/QuickActions.cpp
        )

        add_executable(oneclickrgb_gui ${GUI_SOURCES})
        target_link_libraries(oneclickrgb_gui PRIVATE
            oneclickrgb_core
            $<IF:$<TARGET_EXISTS:Qt6::Widgets>,Qt6::Widgets,Qt5::Widgets>
        )

        install(TARGETS oneclickrgb_gui DESTINATION bin)
    else()
        message(WARNING "Qt not found, GUI will not be built")
    endif()
endif()

# OpenRGB controller integration (optional)
if(USE_OPENRGB_CONTROLLERS)
    # Path to OpenRGB source
    set(OPENRGB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../OpenRGB" CACHE PATH "Path to OpenRGB source")

    if(EXISTS ${OPENRGB_PATH})
        target_include_directories(oneclickrgb_core PRIVATE ${OPENRGB_PATH})
        target_compile_definitions(oneclickrgb_core PRIVATE USE_OPENRGB_CONTROLLERS)
        message(STATUS "Using OpenRGB controllers from: ${OPENRGB_PATH}")
    else()
        message(WARNING "OpenRGB path not found: ${OPENRGB_PATH}")
    endif()
endif()

# Installation
install(DIRECTORY config/profiles DESTINATION share/oneclickrgb)

# Package configuration
set(CPACK_PACKAGE_NAME "OneClickRGB")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Simple One-Click RGB Control")
include(CPack)
