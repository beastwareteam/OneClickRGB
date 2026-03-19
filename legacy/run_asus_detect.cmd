@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > /dev/null 2>&1
if errorlevel 1 (
    echo Failed to setup VS environment
    exit /b 1
)

set QT_PATH=C:\Qt\5.15.0\msvc2019_64

echo === Compiling ASUS Detection Tool ===
cl.exe /nologo /EHsc /std:c++17 /I"%QT_PATH%\include" src\detect_asus_channels.cpp hidapi.lib /Fe:build\detect_asus.exe /link /LIBPATH:"%QT_PATH%\lib" > build\compile_asus.log 2>&1

if errorlevel 1 (
    echo Compilation failed:
    type build\compile_asus.log
    exit /b 1
)

echo Compilation successful!
echo.
echo === Running ASUS Channel Detection ===
echo.
build\detect_asus.exe
