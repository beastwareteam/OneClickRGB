@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"
echo Compiling gpu_info.cpp...
cl /nologo /EHsc /MD /O2 /std:c++17 "D:\xampp\htdocs\RGB\OneClickRGB\src\gpu_info.cpp" /Fe"gpu_info.exe"
if %ERRORLEVEL% EQU 0 (echo SUCCESS) else (echo FAILED)
