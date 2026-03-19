@echo off
REM Compile Channel Settings Dialog

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"

echo Compiling ChannelSettings.exe...
cl /nologo /EHsc /MD /O2 /W3 /std:c++17 /DUNICODE /D_UNICODE ^
    /I"D:\xampp\htdocs\RGB\OneClickRGB\src" ^
    "D:\xampp\htdocs\RGB\OneClickRGB\src\channel_settings_dialog.cpp" ^
    /Fe"ChannelSettings.exe" ^
    /link shell32.lib comctl32.lib user32.lib gdi32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo SUCCESS: ChannelSettings.exe compiled!
    echo Run ChannelSettings.exe to configure per-channel color correction.
) else (
    echo.
    echo FAILED: Compilation errors occurred.
)

echo.
pause
