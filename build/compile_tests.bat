@echo off
REM Compile OneClickRGB Test Suite

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"

echo Compiling test_suite.exe...
cl /nologo /EHsc /MD /O2 /W3 /std:c++17 /DUNICODE /D_UNICODE ^
    /I"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" ^
    "D:\xampp\htdocs\RGB\OneClickRGB\tests\test_suite.cpp" ^
    /Fe"test_suite.exe" ^
    /link /LIBPATH:"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" hidapi.lib shell32.lib

echo Compiling test_integration.exe...
cl /nologo /EHsc /MD /O2 /W3 /std:c++17 /DUNICODE /D_UNICODE ^
    /I"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" ^
    "D:\xampp\htdocs\RGB\OneClickRGB\tests\test_integration.cpp" ^
    /Fe"test_integration.exe" ^
    /link /LIBPATH:"D:\xampp\htdocs\RGB\OneClickRGB\dependencies\hidapi" hidapi.lib

echo Done!
