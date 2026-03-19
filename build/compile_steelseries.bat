@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"
cl /nologo /EHsc /MD /O2 /std:c++17 /I"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" "D:\xampp\htdocs\RGB\OneClickRGB\src\debug_steelseries.cpp" /Fe"D:\xampp\htdocs\RGB\OneClickRGB\build\debug_steelseries.exe" /link /LIBPATH:"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" hidapi.lib setupapi.lib
