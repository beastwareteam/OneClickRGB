#!/usr/bin/env python3
"""
Compile oneclick_rgb.cpp - Standalone unified RGB controller
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path

def build():
    project_dir = Path(__file__).parent
    src_dir = project_dir / "src"
    build_dir = project_dir / "build"
    deps_dir = project_dir / "dependencies"

    build_dir.mkdir(exist_ok=True)

    vs_path = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

    if not os.path.exists(vs_path):
        print("ERROR: Visual Studio Build Tools not found!")
        return 1

    # Single source file
    source = src_dir / "oneclick_rgb.cpp"

    if not source.exists():
        print(f"ERROR: Source file not found: {source}")
        return 1

    print("=" * 60)
    print("OneClickRGB Unified Build")
    print("=" * 60)

    # Build command
    hidapi_inc = deps_dir / "hidapi"
    hidapi_lib = deps_dir / "hidapi"

    compile_cmd = f'cl /nologo /EHsc /MD /O2 /W3 /std:c++17 /DUNICODE /D_UNICODE /I"{hidapi_inc}" "{source}" /Fe"{build_dir / "oneclick_rgb.exe"}" /Fo"{build_dir}\\\\" /link /LIBPATH:"{hidapi_lib}" hidapi.lib setupapi.lib user32.lib kernel32.lib shell32.lib'

    # Create batch file
    batch_file = build_dir / "compile_oneclick.bat"
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

    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)

    exe_path = build_dir / "oneclick_rgb.exe"
    if exe_path.exists():
        # Copy required DLLs
        for dll in ["hidapi.dll", "PawnIOLib.dll"]:
            src_dll = deps_dir / "hidapi" / dll
            if not src_dll.exists():
                src_dll = project_dir / dll
            if src_dll.exists():
                shutil.copy(src_dll, build_dir)
                print(f"Copied: {dll}")

        # Copy SMBus module
        smbus_bin = Path("D:/xampp/htdocs/RGB/OpenRGB/OpenRGB Windows 64-bit/SmbusI801.bin")
        if smbus_bin.exists():
            shutil.copy(smbus_bin, build_dir)
            print("Copied: SmbusI801.bin")

        print("\n" + "=" * 60)
        print("BUILD SUCCESSFUL!")
        print("=" * 60)
        print(f"\nExecutable: {exe_path}")
        print(f"\nUsage: oneclick_rgb.exe <R> <G> <B>")
        print(f"Example: oneclick_rgb.exe 255 0 0  (Red)")
        return 0
    else:
        print("\n" + "=" * 60)
        print("BUILD FAILED!")
        print("=" * 60)
        return 1

if __name__ == "__main__":
    sys.exit(build())
