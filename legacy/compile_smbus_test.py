#!/usr/bin/env python3
"""
Compile script for SMBus scanner/tester
Requires Visual Studio (MSVC) compiler

Run from OneClickRGB directory:
    python compile_smbus_test.py
"""

import subprocess
import os
import sys

# Configuration
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(PROJECT_DIR, "build")
SRC_DIR = os.path.join(PROJECT_DIR, "src")

# Source files
SOURCES = [
    os.path.join(SRC_DIR, "debug_smbus_scan.cpp"),
    os.path.join(SRC_DIR, "smbus", "SMBusWindows.cpp"),
    os.path.join(SRC_DIR, "controllers", "GSkillTridentZ5Controller.cpp"),
    os.path.join(SRC_DIR, "core", "IRGBController.cpp"),
]

# Include directories
INCLUDES = [
    SRC_DIR,
    os.path.join(PROJECT_DIR, "dependencies"),
]

# Output
OUTPUT = os.path.join(BUILD_DIR, "smbus_test.exe")

def find_msvc():
    """Find Visual Studio compiler"""
    # Try vcvarsall locations
    vc_paths = [
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat",
    ]

    for path in vc_paths:
        if os.path.exists(path):
            return path

    return None

def main():
    print("=" * 50)
    print(" OneClickRGB - SMBus Test Compiler")
    print("=" * 50)
    print()

    # Create build directory
    os.makedirs(BUILD_DIR, exist_ok=True)

    # Check for required files
    for src in SOURCES:
        if not os.path.exists(src):
            print(f"ERROR: Source file not found: {src}")
            return 1

    # Find MSVC
    vcvars = find_msvc()
    if not vcvars:
        print("ERROR: Visual Studio not found")
        print("Please install Visual Studio with C++ development tools")
        return 1

    print(f"Using Visual Studio: {vcvars}")

    # Build include flags
    include_flags = " ".join([f'/I"{inc}"' for inc in INCLUDES])

    # Build source list
    source_list = " ".join([f'"{src}"' for src in SOURCES])

    # Compiler flags
    flags = [
        "/EHsc",           # Enable C++ exceptions
        "/std:c++17",      # C++17 standard
        "/W3",             # Warning level 3
        "/O2",             # Optimize for speed
        "/DWIN32",         # Windows platform
        "/D_WINDOWS",
        "/DUNICODE",
        "/D_UNICODE",
    ]

    # Link libraries
    libs = "kernel32.lib user32.lib"

    # Build compile command
    compile_cmd = f'cl.exe {" ".join(flags)} {include_flags} {source_list} /Fe:"{OUTPUT}" /link {libs}'

    # Create batch file to run with vcvars
    batch_content = f'''@echo off
call "{vcvars}"
cd /d "{PROJECT_DIR}"
echo.
echo Compiling SMBus test...
echo.
{compile_cmd}
echo.
if exist "{OUTPUT}" (
    echo SUCCESS: {OUTPUT}
) else (
    echo FAILED: Compilation error
)
'''

    batch_file = os.path.join(BUILD_DIR, "compile_smbus.bat")
    with open(batch_file, "w") as f:
        f.write(batch_content)

    print(f"\nCompiling...")
    print(f"Sources: {len(SOURCES)} files")
    print()

    # Run the batch file
    result = subprocess.run(["cmd", "/c", batch_file], cwd=PROJECT_DIR)

    if result.returncode == 0 and os.path.exists(OUTPUT):
        print()
        print("=" * 50)
        print(" Compilation successful!")
        print("=" * 50)
        print()
        print(f"Output: {OUTPUT}")
        print()
        print("Usage (run as Administrator):")
        print(f"  {OUTPUT} scan           - Scan SMBus for devices")
        print(f"  {OUTPUT} dump 0x50      - Dump DIMM0 registers")
        print(f"  {OUTPUT} test 0x50      - Test RGB control on DIMM0")
        print()
        print("IMPORTANT: Requires WinRing0x64.dll and WinRing0x64.sys")
        print("           in the same directory as the executable.")
        return 0
    else:
        print()
        print("Compilation failed. Check errors above.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
