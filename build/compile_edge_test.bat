@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"
cl /nologo /EHsc /MD /O2 /I"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" "D:\xampp\htdocs\RGB\OneClickRGB\src\evision_edge_test.cpp" /Fe"evision_edge_test.exe" /link /LIBPATH:"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" hidapi.lib
