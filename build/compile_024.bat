@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"
cl /nologo /EHsc /MD /O2 /W3 /std:c++17 /DUNICODE /D_UNICODE /I"D:\xampp\htdocs\RGB\OneClickRGB\src" "D:\xampp\htdocs\RGB\OneClickRGB\src\channel_settings_dialog.cpp" /Fe"D:\xampp\htdocs\RGB\OneClickRGB\build\ChannelSettings.exe" /link shell32.lib comctl32.lib user32.lib gdi32.lib
