#!/usr/bin/env python3
"""
OneClickRGB Build Script
Uses Visual Studio 2019 Build Tools
"""

import os
import sys
import subprocess
from pathlib import Path

def main():
    # Paths
    project_dir = Path(__file__).parent
    src_dir = project_dir / "src"
    build_dir = project_dir / "build"

    # Create build directory
    build_dir.mkdir(exist_ok=True)

    # VS environment setup
    vs_path = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

    if not os.path.exists(vs_path):
        print("ERROR: Visual Studio Build Tools not found!")
        return 1

    # Source files
    sources = [
        src_dir / "main.cpp",
        src_dir / "core" / "OneClickRGB.cpp",
        src_dir / "core" / "DeviceManager.cpp",
        src_dir / "core" / "ProfileManager.cpp",
        src_dir / "core" / "ConfigManager.cpp",
        src_dir / "core" / "AutoStart.cpp",
        src_dir / "scanner" / "HardwareScanner.cpp",
        src_dir / "devices" / "RGBDevice.cpp",
        src_dir / "devices" / "HIDController.cpp",
        src_dir / "controllers" / "AsusAuraMainboardController.cpp",
        src_dir / "controllers" / "SteelSeriesRival600Controller.cpp",
        src_dir / "controllers" / "EVisionKeyboardController.cpp",
    ]

    print("="*60)
    print("OneClickRGB Build")
    print("="*60)
    print("\nChecking source files...")

    for src in sources:
        if src.exists():
            print(f"  [OK] {src.name}")
        else:
            print(f"  [MISSING] {src}")
            return 1

    output = build_dir / "oneclickrgb.exe"


    # Create response file for compiler (no link options - those go on command line)
    rsp_file = build_dir / "compile.rsp"
    with open(rsp_file, 'w') as f:
        f.write("/nologo\n")
        f.write("/EHsc\n")
        f.write("/std:c++17\n")
        f.write("/O2\n")
        f.write("/MD\n")
        f.write("/W3\n")
        f.write("/DWIN32\n")
        f.write("/D_WINDOWS\n")
        f.write("/DUNICODE\n")
        f.write("/D_UNICODE\n")
        f.write(f'/I"{src_dir}"\n')
        f.write(f'/I"{src_dir / "controllers"}"\n')
        f.write(f'/I"{project_dir / "dependencies"}"\n')
        f.write(f'/I"{project_dir / "dependencies" / "hidapi"}"\n')
        f.write(f'/Fe"{output}"\n')
        for src in sources:
            f.write(f'"{src}"\n')

    print(f"\nCompiling...")
    print(f"Output: {output}\n")

    # Run with VS environment - link options inline
    hidapi_lib = project_dir / "dependencies" / "hidapi"
    cmd = f'call "{vs_path}" >nul 2>&1 && cl.exe @"{rsp_file}" /link /LIBPATH:"{hidapi_lib}" shell32.lib advapi32.lib hidapi.lib setupapi.lib'

    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True,
        cwd=str(project_dir)
    )

    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)

    if result.returncode != 0:
        print("\n" + "="*60)
        print("BUILD FAILED")
        print("="*60)
        return 1

    # Copy hidapi.dll to build directory
    import shutil
    hidapi_dll = project_dir / "dependencies" / "hidapi" / "hidapi.dll"
    if hidapi_dll.exists():
        shutil.copy(hidapi_dll, build_dir / "hidapi.dll")
        print("Copied: hidapi.dll")

    print("\n" + "="*60)
    print("BUILD SUCCESSFUL!")
    print("="*60)
    print(f"\nExecutable: {output}")

    return 0

if __name__ == "__main__":
    sys.exit(main())
