cmake_minimum_required(VERSION 3.16)
project(OneClickRGB VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_GUI "Build Qt GUI application" ON)
option(BUILD_CLI "Build command-line interface" ON)
option(USE_OPENRGB_CONTROLLERS "Use OpenRGB controller implementations" OFF)

# ==================== BUNDLED DEPENDENCIES ====================
# Always use bundled dependencies for portability

# HIDAPI - bundled in dependencies/hidapi/
set(HIDAPI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/hidapi")
set(HIDAPI_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/hidapi/hidapi.lib")

if(EXISTS "${HIDAPI_LIBRARY}")
    message(STATUS "Using bundled HIDAPI from: ${HIDAPI_INCLUDE_DIR}")
    add_library(hidapi INTERFACE)
    target_include_directories(hidapi INTERFACE ${HIDAPI_INCLUDE_DIR})
    target_link_libraries(hidapi INTERFACE ${HIDAPI_LIBRARY})
    set(HIDAPI_FOUND TRUE)
else()
    find_package(hidapi QUIET)
    if(NOT hidapi_FOUND)
        message(WARNING "HIDAPI not found - device detection will not work")
    endif()
endif()

# nlohmann_json - bundled in dependencies/nlohmann/
message(STATUS "Using bundled nlohmann_json")
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/nlohmann
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies)

# Source files
set(CORE_SOURCES
    src/core/DeviceManager.cpp
    src/core/ProfileManager.cpp
    src/core/OneClickRGB.cpp
    src/core/AutoStart.cpp
    src/core/ConfigManager.cpp
    src/core/DeviceRegistry.cpp
    src/core/modules/ModuleManager.cpp
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
if(HIDAPI_FOUND)
    target_link_libraries(oneclickrgb_core PUBLIC hidapi)
    target_include_directories(oneclickrgb_core PUBLIC ${HIDAPI_INCLUDE_DIR})
elseif(hidapi_FOUND)
    target_link_libraries(oneclickrgb_core PUBLIC hidapi::hidapi)
else()
    message(WARNING "HIDAPI not available - using setupapi only")
    if(WIN32)
        target_link_libraries(oneclickrgb_core PUBLIC setupapi)
    endif()
endif()

target_link_libraries(oneclickrgb_core PUBLIC nlohmann_json)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(oneclickrgb_core PUBLIC shell32)
endif()

# ==================== MODULES ====================

# ASUS Aura Module
add_library(asus_aura_module SHARED
    src/modules/AsusAuraModule.cpp
    src/controllers/AsusAuraMainboardController.cpp
)

target_link_libraries(asus_aura_module PRIVATE oneclickrgb_core)
set_target_properties(asus_aura_module PROPERTIES
    OUTPUT_NAME "asus_aura"
    PREFIX ""
)

# SteelSeries Module
add_library(steelseries_module SHARED
    src/modules/SteelSeriesModule.cpp
    src/controllers/SteelSeriesRival600Controller.cpp
)

target_link_libraries(steelseries_module PRIVATE oneclickrgb_core)
set_target_properties(steelseries_module PROPERTIES
    OUTPUT_NAME "steelseries"
    PREFIX ""
)

# Install modules
install(TARGETS asus_aura_module steelseries_module
    LIBRARY DESTINATION modules
    RUNTIME DESTINATION modules
)

# CLI Application
if(BUILD_CLI)
    add_executable(oneclickrgb src/main.cpp)
    target_link_libraries(oneclickrgb PRIVATE oneclickrgb_core)

    install(TARGETS oneclickrgb DESTINATION bin)
endif()

# GUI Application (Qt)
if(BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets Core QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt5 COMPONENTS Widgets Core QUIET)
        if(Qt5_FOUND)
            set(QT_VERSION 5)
            message(STATUS "Using Qt5 (Qt6 not found)")
        else()
            message(WARNING "Qt not found - GUI will not be built")
            set(BUILD_GUI OFF)
        endif()
    else()
        set(QT_VERSION 6)
        message(STATUS "Using Qt6")
    endif()

    if(BUILD_GUI)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTOUIC ON)
        set(CMAKE_AUTORCC ON)

        # Qt resources
        set(QT_RESOURCES
            resources/resources.qrc
        )

        set(GUI_SOURCES
            src/ui/MainWindow.cpp
            src/ui/DeviceCard.cpp
            src/ui/QuickActions.cpp
            src/ui/ColorPicker.cpp
            src/ui/ProfileWidget.cpp
            src/ui/SettingsDialog.cpp
            ${QT_RESOURCES}
        )

        if(QT_VERSION EQUAL 6)
            qt6_add_executable(oneclickrgb_gui src/main_gui.cpp ${GUI_SOURCES})
            target_link_libraries(oneclickrgb_gui PRIVATE oneclickrgb_core Qt6::Widgets Qt6::Core)
        else()
            add_executable(oneclickrgb_gui src/main_gui.cpp ${GUI_SOURCES})
            target_link_libraries(oneclickrgb_gui PRIVATE oneclickrgb_core Qt5::Widgets Qt5::Core)
        endif()

        # Set executable properties
        set_target_properties(oneclickrgb_gui PROPERTIES
            OUTPUT_NAME "OneClickRGB"
            WIN32_EXECUTABLE TRUE
        )

        install(TARGETS oneclickrgb_gui DESTINATION bin)
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
