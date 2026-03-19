@echo off
call "D:\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\xampp\htdocs\RGB\OneClickRGB\build
cl /nologo /EHsc /MD /O2 /W3 /std:c++17 /DUNICODE /D_UNICODE /ID:\xampp\htdocs\RGB\OneClickRGB\src /ID:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi D:\xampp\htdocs\RGB\OneClickRGB\src\oneclick_rgb_complete.cpp /FeOneClickRGB.exe /link /LIBPATH:D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi hidapi.lib shell32.lib comctl32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib
