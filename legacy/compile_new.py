#!/usr/bin/env python3
"""
OneClickRGB Build Script - New Architecture
Uses Visual Studio 2019 Build Tools
"""

import os
import sys
import subprocess
from pathlib import Path

def build_new():
    # Paths
    project_dir = Path(__file__).parent
    src_dir = project_dir / "src"
    build_dir = project_dir / "build"
    deps_dir = project_dir / "dependencies"

    # Create build directory
    build_dir.mkdir(exist_ok=True)

    # VS environment setup
    vs_path = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

    if not os.path.exists(vs_path):
        print("ERROR: Visual Studio Build Tools not found!")
        return 1

    # Source files - New Architecture
    sources = [
        src_dir / "main_new.cpp",
        src_dir / "core" / "IRGBController.cpp",
        src_dir / "core" / "DeviceDatabase.cpp",
        src_dir / "core" / "ControllerFactory.cpp",
        src_dir / "controllers" / "AsusAuraUSBController.cpp",
        src_dir / "controllers" / "SteelSeriesRivalController.cpp",
        src_dir / "controllers" / "EVisionKeyboardController.cpp",
    ]

    print("=" * 60)
    print("OneClickRGB Build - New Architecture")
    print("=" * 60)
    print("\nChecking source files...")

    for src in sources:
        if src.exists():
            print(f"  OK: {src.name}")
        else:
            print(f"  MISSING: {src}")
            return 1

    # Include directories
    includes = [
        deps_dir / "hidapi",  # hidapi.h is directly here
        src_dir,
        src_dir / "core",
        src_dir / "controllers",
    ]

    # Build command
    include_flags = " ".join([f'/I"{inc}"' for inc in includes])
    source_files = " ".join([f'"{src}"' for src in sources])
    lib_path = deps_dir / "hidapi"  # hidapi.lib is directly here

    compile_cmd = f'cl /nologo /EHsc /MD /O2 /W3 /std:c++20 /DUNICODE /D_UNICODE {include_flags} {source_files} /Fe"{build_dir / "oneclickrgb_new.exe"}" /Fo"{build_dir}\\\\" /link /LIBPATH:"{lib_path}" hidapi.lib setupapi.lib user32.lib kernel32.lib'

    # Create batch file for compilation
    batch_file = build_dir / "compile_new.bat"
    with open(batch_file, "w") as f:
        f.write(f'@echo off\n')
        f.write(f'call "{vs_path}"\n')
        f.write(f'cd /d "{build_dir}"\n')
        f.write(compile_cmd + "\n")

    print("\nCompiling...")
    result = subprocess.run(
        str(batch_file),
        shell=True,
        capture_output=True,
        text=True,
        cwd=str(build_dir)
    )

    # Print output
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)

    # Check for executable
    exe_path = build_dir / "oneclickrgb_new.exe"
    if exe_path.exists():
        # Copy hidapi.dll
        hidapi_dll = deps_dir / "hidapi" / "hidapi.dll"
        if hidapi_dll.exists():
            import shutil
            shutil.copy(hidapi_dll, build_dir)
            print("Copied: hidapi.dll")

        print("\n" + "=" * 60)
        print("BUILD SUCCESSFUL!")
        print("=" * 60)
        print(f"\nExecutable: {exe_path}")
        return 0
    else:
        print("\n" + "=" * 60)
        print("BUILD FAILED!")
        print("=" * 60)
        return 1


if __name__ == "__main__":
    sys.exit(build_new())
