#!/usr/bin/env python3
"""
OneClickRGB GUI Build Script
Uses Visual Studio 2019 + Qt 5.15.0
"""

import os
import sys
import subprocess
from pathlib import Path

def main():
    project_dir = Path(__file__).parent
    src_dir = project_dir / "src"
    build_dir = project_dir / "build"
    build_dir.mkdir(exist_ok=True)

    # Paths
    vs_path = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    qt_dir = Path(r"C:\Qt\5.15.0\msvc2019_64")
    moc = qt_dir / "bin" / "moc.exe"

    if not os.path.exists(vs_path):
        print("ERROR: Visual Studio Build Tools not found!")
        return 1

    if not qt_dir.exists():
        print(f"ERROR: Qt not found at {qt_dir}")
        return 1

    print("="*60)
    print("OneClickRGB GUI Build")
    print("="*60)

    # Step 1: Run MOC on MainWindow.h
    print("\n[1/3] Running Qt MOC...")
    moc_output = build_dir / "moc_MainWindow.cpp"
    moc_input = src_dir / "ui" / "MainWindow.h"

    moc_cmd = f'"{moc}" "{moc_input}" -o "{moc_output}"'
    result = subprocess.run(moc_cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"MOC Error: {result.stderr}")
        return 1
    print(f"  Generated: {moc_output.name}")

    # Step 2: Compile
    print("\n[2/3] Compiling...")

    sources = [
        src_dir / "main_gui.cpp",
        src_dir / "ui" / "MainWindow.cpp",
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
        moc_output,
    ]

    output = build_dir / "oneclickrgb_gui.exe"

    # Create response file
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
        # Include paths
        f.write(f'/I"{src_dir}"\n')
        f.write(f'/I"{src_dir / "controllers"}"\n')
        f.write(f'/I"{project_dir / "dependencies"}"\n')
        f.write(f'/I"{project_dir / "dependencies" / "hidapi"}"\n')
        f.write(f'/I"{qt_dir / "include"}"\n')
        f.write(f'/I"{qt_dir / "include" / "QtCore"}"\n')
        f.write(f'/I"{qt_dir / "include" / "QtGui"}"\n')
        f.write(f'/I"{qt_dir / "include" / "QtWidgets"}"\n')
        f.write(f'/Fe"{output}"\n')
        # Sources
        for src in sources:
            f.write(f'"{src}"\n')
    # Build command with link options inline
    qt_lib = qt_dir / "lib"
    hidapi_lib = project_dir / "dependencies" / "hidapi"

    cmd = f'''call "{vs_path}" >nul 2>&1 && cl.exe @"{rsp_file}" /link /SUBSYSTEM:WINDOWS /LIBPATH:"{qt_lib}" /LIBPATH:"{hidapi_lib}" shell32.lib advapi32.lib setupapi.lib hidapi.lib Qt5Core.lib Qt5Gui.lib Qt5Widgets.lib'''

    result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=str(project_dir))

    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)

    if result.returncode != 0:
        print("\n" + "="*60)
        print("BUILD FAILED")
        print("="*60)
        return 1

    # Step 3: Copy DLLs
    print("\n[3/3] Copying DLLs...")

    # Copy hidapi.dll
    import shutil
    hidapi_dll = project_dir / "dependencies" / "hidapi" / "hidapi.dll"
    if hidapi_dll.exists():
        shutil.copy(hidapi_dll, build_dir / "hidapi.dll")
        print("  Copied: hidapi.dll")

    # Copy Qt DLLs
    dlls = ["Qt5Core.dll", "Qt5Gui.dll", "Qt5Widgets.dll"]
    for dll in dlls:
        src_dll = qt_dir / "bin" / dll
        dst_dll = build_dir / dll
        if src_dll.exists() and not dst_dll.exists():
            import shutil
            shutil.copy(src_dll, dst_dll)
            print(f"  Copied: {dll}")

    # Copy platforms plugin
    platforms_dir = build_dir / "platforms"
    platforms_dir.mkdir(exist_ok=True)
    qwindows = qt_dir / "plugins" / "platforms" / "qwindows.dll"
    if qwindows.exists():
        import shutil
        shutil.copy(qwindows, platforms_dir / "qwindows.dll")
        print("  Copied: platforms/qwindows.dll")

    print("\n" + "="*60)
    print("BUILD SUCCESSFUL!")
    print("="*60)
    print(f"\nExecutable: {output}")
    print(f"\nRun: {output}")

    return 0

if __name__ == "__main__":
    sys.exit(main())
