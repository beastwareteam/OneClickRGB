@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "D:\xampp\htdocs\RGB\OneClickRGB"
cl.exe /EHsc /std:c++17 /Idependencies\hidapi src\evision_lock_test.cpp dependencies\hidapi\hidapi.lib /Fe:build\evision_lock_test.exe
