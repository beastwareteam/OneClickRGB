@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"
cl /nologo /EHsc /MD /O2 /std:c++17 "D:\xampp\htdocs\RGB\OneClickRGB\src\gpu_info.cpp" /Fe"gpu_info.exe" 2>&1
if exist gpu_info.exe (
    echo.
    echo Running gpu_info.exe...
    echo.
    gpu_info.exe
) else (
    echo Compile failed!
)
