import subprocess
import os

# Qt and compiler paths
QT_PATH = "C:/Qt/5.15.0/msvc2019_64"
VS_PATH = "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community"

# Setup environment
env = os.environ.copy()
env["PATH"] = f"{QT_PATH}/bin;{VS_PATH}/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x64;{VS_PATH}/Common7/IDE;" + env["PATH"]
env["INCLUDE"] = f"{VS_PATH}/VC/Tools/MSVC/14.29.30133/include;C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/ucrt;C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/um;C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/shared;{QT_PATH}/include"
env["LIB"] = f"{VS_PATH}/VC/Tools/MSVC/14.29.30133/lib/x64;C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/ucrt/x64;C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64;{QT_PATH}/lib"

os.makedirs("build", exist_ok=True)

# Compile
print("Compiling ASUS detection tool...")
result = subprocess.run([
    "cl.exe", "/EHsc", "/std:c++17",
    f"/I{QT_PATH}/include",
    "src/detect_asus_channels.cpp",
    "hidapi.lib",
    "/Fe:build/detect_asus.exe",
    "/link", f"/LIBPATH:{QT_PATH}/lib"
], env=env, capture_output=True, text=True)

print(result.stdout)
if result.returncode != 0:
    print("STDERR:", result.stderr)
else:
    print("\n=== Running detection ===\n")
    run = subprocess.run(["build/detect_asus.exe"], capture_output=True, text=True)
    print(run.stdout)
    if run.stderr:
        print("STDERR:", run.stderr)

