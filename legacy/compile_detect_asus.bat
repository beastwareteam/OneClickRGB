@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set QT_PATH=C:\Qt\5.15.0\msvc2019_64
set INCLUDE=%INCLUDE%;%QT_PATH%\include

if not exist build mkdir build

echo Compiling ASUS detection tool...
cl.exe /EHsc /std:c++17 /I"%QT_PATH%\include" src\detect_asus_channels.cpp hidapi.lib /Fe:build\detect_asus.exe /link /LIBPATH:"%QT_PATH%\lib"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo === Running detection ===
    echo.
    build\detect_asus.exe
)
