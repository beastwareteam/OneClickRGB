@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "D:\xampp\htdocs\RGB\OneClickRGB"
cl.exe /EHsc /std:c++17 /Idependencies\hidapi src\oneclick_rgb.cpp dependencies\hidapi\hidapi.lib Shell32.lib /Fe:build\oneclick_rgb_unified.exe
echo.
echo Done!
pause
