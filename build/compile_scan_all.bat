@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"
cl /nologo /EHsc /MD /O2 /W3 /std:c++17 /DUNICODE /D_UNICODE /I"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" "D:\xampp\htdocs\RGB\OneClickRGB\src\evision_scan_all.cpp" /Fe"D:\xampp\htdocs\RGB\OneClickRGB\build\evision_scan_all.exe" /link /LIBPATH:"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" hidapi.lib
