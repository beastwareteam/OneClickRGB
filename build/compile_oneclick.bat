@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"
cl /nologo /EHsc /MD /O2 /W3 /std:c++17 /DUNICODE /D_UNICODE /I"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" "D:\xampp\htdocs\RGB\OneClickRGB\src\oneclick_rgb.cpp" /Fe"D:\xampp\htdocs\RGB\OneClickRGB\build\oneclick_rgb.exe" /Fo"D:\xampp\htdocs\RGB\OneClickRGB\build\\" /link /LIBPATH:"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" hidapi.lib setupapi.lib user32.lib kernel32.lib shell32.lib
