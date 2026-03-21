#!/usr/bin/env python3
"""
OneClickRGB GUI Build Script
Uses Visual Studio 2019 Build Tools with Qt6
"""

import os
import sys
import subprocess
from pathlib import Path

def main():
    # Paths
    project_dir = Path(__file__).parent.parent  # Go up one level from production/
    src_dir = project_dir / "src"
    build_dir = project_dir / "production" / "build"

    # Create build directory
    build_dir.mkdir(exist_ok=True)

    # VS environment setup
    vs_path = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

    if not os.path.exists(vs_path):
        print("ERROR: Visual Studio Build Tools not found!")
        return 1

    # Qt paths (try Qt6 first, fallback to Qt5)
    qt6_path = Path(r"C:\Qt\6.5.0\msvc2019_64")
    qt5_path = Path(r"C:\Qt\5.15.0\msvc2019_64")

    qt_include = ""
    qt_lib = ""

    if qt6_path.exists():
        qt_include = str(qt6_path / "include")
        qt_lib = str(qt6_path / "lib")
        print("Using Qt6")
    elif qt5_path.exists():
        qt_include = str(qt5_path / "include")
        qt_lib = str(qt5_path / "lib")
        print("Using Qt5 (Qt6 not found)")
    else:
        print("ERROR: Qt not found! Please install Qt6 or Qt5.")
        print(f"Checked paths: {qt6_path}, {qt5_path}")
        return 1

    # Source files (CLI + GUI)
    sources = [
        # Core
        src_dir / "main_gui.cpp",
        src_dir / "core" / "OneClickRGB.cpp",
        src_dir / "core" / "DeviceManager.cpp",
        src_dir / "core" / "ProfileManager.cpp",
        src_dir / "core" / "ConfigManager.cpp",
        src_dir / "core" / "AutoStart.cpp",
        src_dir / "scanner" / "HardwareScanner.cpp",
        src_dir / "devices" / "RGBDevice.cpp",
        src_dir / "devices" / "HIDController.cpp",

        # Modules
        src_dir / "core" / "modules" / "ModuleManager.cpp",
        src_dir / "modules" / "AsusAuraModule.cpp",
        src_dir / "modules" / "SteelSeriesModule.cpp",

        # GUI
        src_dir / "ui" / "MainWindow.cpp",
        src_dir / "ui" / "DeviceCard.cpp",
        src_dir / "ui" / "QuickActions.cpp",
        src_dir / "ui" / "SettingsDialog.cpp",
    ]

    print("="*60)
    print("OneClickRGB GUI Build")
    print("="*60)
    print("\nChecking source files...")

    for src in sources:
        if src.exists():
            print(f"  [OK] {src.name}")
        else:
            print(f"  [MISSING] {src}")
            return 1

    output = build_dir / "OneClickRGB.exe"

    # Create response file for compiler
    rsp_file = build_dir / "compile_gui.rsp"
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
        f.write("/DQT_NO_DEBUG\n")
        f.write("/DQT_WIDGETS_LIB\n")
        f.write("/DQT_GUI_LIB\n")
        f.write("/DQT_CORE_LIB\n")
        f.write(f'/I"{src_dir}"\n')
        f.write(f'/I"{src_dir / "controllers"}"\n')
        f.write(f'/I"{project_dir / "dependencies"}"\n')
        f.write(f'/I"{project_dir / "dependencies" / "hidapi"}"\n')
        f.write(f'/I"{qt_include}"\n')
        f.write(f'/I"{qt_include}\\QtWidgets"\n')
        f.write(f'/I"{qt_include}\\QtGui"\n')
        f.write(f'/I"{qt_include}\\QtCore"\n')
        f.write(f'/Fe"{output}"\n')
        for src in sources:
            f.write(f'"{src}"\n')

    print(f"\nCompiling GUI application...")
    print(f"Output: {output}\n")

    # Run with VS environment
    hidapi_lib = project_dir / "dependencies" / "hidapi"
    qt_lib_path = qt_lib

    # Link libraries
    if qt6_path.exists():
        link_libs = "Qt6Widgets.lib Qt6Gui.lib Qt6Core.lib"
    else:
        link_libs = "Qt5Widgets.lib Qt5Gui.lib Qt5Core.lib"

    cmd = f'call "{vs_path}" >nul 2>&1 && cl.exe @"{rsp_file}" /link /LIBPATH:"{hidapi_lib}" /LIBPATH:"{qt_lib_path}" {link_libs} shell32.lib advapi32.lib hidapi.lib setupapi.lib user32.lib gdi32.lib comdlg32.lib oleaut32.lib imm32.lib winmm.lib ws2_32.lib ole32.lib uuid.lib'

    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True,
        encoding='utf-8',
        errors='ignore',
        cwd=str(project_dir)
    )

    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)

    if result.returncode != 0:
        print("\n" + "="*60)
        print("GUI BUILD FAILED")
        print("="*60)
        return 1

    # Copy required DLLs
    import shutil

    # hidapi.dll
    hidapi_dll = project_dir / "dependencies" / "hidapi" / "hidapi.dll"
    if hidapi_dll.exists():
        shutil.copy(hidapi_dll, build_dir / "hidapi.dll")
        print("Copied: hidapi.dll")

    # Qt DLLs
    qt_bin = qt6_path / "bin" if qt6_path.exists() else qt5_path / "bin"
    qt_dlls = []
    if qt6_path.exists():
        qt_dlls = ["Qt6Widgets.dll", "Qt6Gui.dll", "Qt6Core.dll"]
    else:
        qt_dlls = ["Qt5Widgets.dll", "Qt5Gui.dll", "Qt5Core.dll"]

    for dll in qt_dlls:
        dll_path = qt_bin / dll
        if dll_path.exists():
            shutil.copy(dll_path, build_dir / dll)
            print(f"Copied: {dll}")

    print("\n" + "="*60)
    print("GUI BUILD SUCCESSFUL!")
    print("="*60)
    print(f"\nExecutable: {output}")

    return 0

if __name__ == "__main__":
    sys.exit(main())