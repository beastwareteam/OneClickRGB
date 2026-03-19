@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "D:\xampp\htdocs\RGB\OneClickRGB"
cl.exe /EHsc /std:c++17 src\ene_debug.cpp /Fe:build\ene_debug.exe
