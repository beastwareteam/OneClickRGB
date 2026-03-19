@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "D:\xampp\htdocs\RGB\OneClickRGB"
cl.exe /EHsc /std:c++17 /Idependencies\hidapi src\test_edge_toggle.cpp dependencies\hidapi\hidapi.lib /Fe:build\test_edge_toggle.exe
