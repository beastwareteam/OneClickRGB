@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "D:\xampp\htdocs\RGB\OneClickRGB\build"
cl.exe /EHsc /std:c++17 /I..\dependencies\hidapi restore_state.cpp ..\dependencies\hidapi\hidapi.lib /Fe:restore_state.exe
