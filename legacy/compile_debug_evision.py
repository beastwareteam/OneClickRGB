#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path

project_dir = Path(__file__).parent
src_dir = project_dir / "src"
build_dir = project_dir / "build"
deps_dir = project_dir / "dependencies"

vs_path = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

source = src_dir / "debug_evision_v2.cpp"
output = build_dir / "debug_evision.exe"

includes = [deps_dir / "hidapi"]
lib_path = deps_dir / "hidapi"

include_flags = " ".join([f'/I"{inc}"' for inc in includes])

compile_cmd = f'cl /nologo /EHsc /MD /O2 /std:c++17 {include_flags} "{source}" /Fe"{output}" /link /LIBPATH:"{lib_path}" hidapi.lib setupapi.lib'

batch_file = build_dir / "compile_debug.bat"
with open(batch_file, "w") as f:
    f.write(f'@echo off\n')
    f.write(f'call "{vs_path}"\n')
    f.write(f'cd /d "{build_dir}"\n')
    f.write(compile_cmd + "\n")

print("Compiling EVision debug tool...")
result = subprocess.run(str(batch_file), shell=True, capture_output=True, text=True, cwd=str(build_dir))

if result.stdout:
    print(result.stdout)

if output.exists():
    print(f"SUCCESS: {output}")
else:
    print("FAILED!")
    if result.stderr:
        print(result.stderr)
