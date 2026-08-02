cmake_minimum_required(VERSION 3.20)
project(OneClickRGB VERSION 3.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ONECLICKRGB_BUILD_APP   "Build the Win32 GUI application" ${WIN32})
option(ONECLICKRGB_BUILD_TESTS "Build the unit tests"            ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra)
endif()

#------------------------------------------------------------------------------
# oneclickrgb_core
#
# Device protocols, profile storage and autostart - everything that is not the
# Win32 window. This is the library the tests link against; it deliberately does
# not pull in hidapi or PawnIO, only the interfaces in src/hal.
#------------------------------------------------------------------------------
add_library(oneclickrgb_core STATIC
    src/devices/asus_aura.cpp
    src/devices/evision.cpp
    src/devices/gskill_ram.cpp
    src/devices/steelseries.cpp
    src/hal/smbus_backend.cpp
    src/profile.cpp
)

target_include_directories(oneclickrgb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Autostart is Task Scheduler plumbing and only exists on Windows.
if(WIN32)
    target_sources(oneclickrgb_core PRIVATE src/autostart.cpp)
endif()

#------------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------------
if(ONECLICKRGB_BUILD_TESTS)
    enable_testing()

    set(TEST_SOURCES
        tests/test_main.cpp
        tests/test_asus_aura.cpp
        tests/test_evision.cpp
        tests/test_gskill_ram.cpp
        tests/test_profile.cpp
        tests/test_steelseries.cpp
    )
    if(WIN32)
        list(APPEND TEST_SOURCES tests/test_autostart.cpp)
    endif()

    add_executable(oneclickrgb_tests ${TEST_SOURCES})
    target_link_libraries(oneclickrgb_tests PRIVATE oneclickrgb_core)
    target_include_directories(oneclickrgb_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    add_test(NAME unit_tests COMMAND oneclickrgb_tests)
endif()

#------------------------------------------------------------------------------
# Application
#
# The GUI still lives in one translation unit; it links the core library rather
# than carrying its own copies of the protocols.
#------------------------------------------------------------------------------
if(ONECLICKRGB_BUILD_APP AND WIN32)
    add_executable(OneClickRGB WIN32
        src/oneclick_rgb_complete.cpp
        src/hal/hid_backend_hidapi.cpp
        src/hal/smbus_backend_pawnio.cpp
        src/OneClickRGB.rc
    )

    target_link_libraries(OneClickRGB PRIVATE oneclickrgb_core)

    target_include_directories(OneClickRGB PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/hidapi
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies
    )

    target_link_libraries(OneClickRGB PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/hidapi/hidapi.lib
        comctl32 comdlg32 shell32 ole32 gdiplus wtsapi32 powrprof
        uxtheme dwmapi advapi32 user32 gdi32
    )

    set_target_properties(OneClickRGB PROPERTIES
        LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
    )
endif()
