@echo off
REM ================================================
REM OneClickRGB - Native GUI Build Script
REM Builds WITHOUT Qt dependency (Win32 GUI)
REM ================================================
setlocal enabledelayedexpansion

echo.
echo ================================================
echo OneClickRGB Native Build (No Qt Required)
echo ================================================
echo.

REM Find Visual Studio
set VS_FOUND=0

REM Try VS 2022
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    set VS_FOUND=1
    echo Found: Visual Studio 2022 Community
    goto :vs_found
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    set VS_FOUND=1
    echo Found: Visual Studio 2022 Professional
    goto :vs_found
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    set VS_FOUND=1
    echo Found: Visual Studio 2022 Build Tools
    goto :vs_found
)

REM Try VS 2019
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    set VS_FOUND=1
    echo Found: Visual Studio 2019 Community
    goto :vs_found
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    set VS_FOUND=1
    echo Found: Visual Studio 2019 Professional
    goto :vs_found
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    set VS_FOUND=1
    echo Found: Visual Studio 2019 Build Tools
    goto :vs_found
)

if %VS_FOUND%==0 (
    echo ERROR: Visual Studio not found!
    echo.
    echo Please install one of:
    echo   - Visual Studio 2019/2022 Community (free)
    echo   - Visual Studio Build Tools
    echo.
    echo Download: https://visualstudio.microsoft.com/downloads/
    exit /b 1
)

:vs_found
echo.

REM Set paths relative to script location
set ROOT_DIR=%~dp0
set SRC_DIR=%ROOT_DIR%src
set BUILD_DIR=%ROOT_DIR%build
set DEP_DIR=%ROOT_DIR%dependencies

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Compiling OneClickRGB (Native GUI)...
echo.

REM Source files.
REM The Win32 front end plus the device protocol modules, the profile store,
REM the autostart task and the two real HAL backends. Keep this list in sync
REM with the OneClickRGB target in CMakeLists.txt.
set SOURCE="%SRC_DIR%\oneclick_rgb_complete.cpp"
set SOURCE=%SOURCE% "%SRC_DIR%\devices\asus_aura.cpp"
set SOURCE=%SOURCE% "%SRC_DIR%\devices\evision.cpp"
set SOURCE=%SOURCE% "%SRC_DIR%\devices\gskill_ram.cpp"
set SOURCE=%SOURCE% "%SRC_DIR%\devices\steelseries.cpp"
set SOURCE=%SOURCE% "%SRC_DIR%\hal\hid_backend_hidapi.cpp"
set SOURCE=%SOURCE% "%SRC_DIR%\hal\smbus_backend.cpp"
set SOURCE=%SOURCE% "%SRC_DIR%\hal\smbus_backend_pawnio.cpp"
set SOURCE=%SOURCE% "%SRC_DIR%\autostart.cpp"
set SOURCE=%SOURCE% "%SRC_DIR%\profile.cpp"

if not exist "%SRC_DIR%\oneclick_rgb_complete.cpp" (
    echo ERROR: Source file not found: oneclick_rgb_complete.cpp
    exit /b 1
)

REM Compiler flags
set CFLAGS=/nologo /EHsc /std:c++17 /O2 /MD /W3
set CFLAGS=%CFLAGS% /DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE

REM Include paths
set INCLUDES=/I"%SRC_DIR%" /I"%DEP_DIR%\hidapi" /I"%DEP_DIR%"

REM Libraries
set LIBS=shell32.lib advapi32.lib setupapi.lib comctl32.lib comdlg32.lib user32.lib gdi32.lib
set LIBS=%LIBS% ole32.lib gdiplus.lib wtsapi32.lib powrprof.lib uxtheme.lib dwmapi.lib
set LIBS=%LIBS% "%DEP_DIR%\hidapi\hidapi.lib"

REM Resource file (icon)
set RES_FILE=
if exist "%SRC_DIR%\OneClickRGB.rc" (
    echo Compiling resources...
    rc /nologo /fo"%BUILD_DIR%\OneClickRGB.res" "%SRC_DIR%\OneClickRGB.rc" >nul 2>&1
    if exist "%BUILD_DIR%\OneClickRGB.res" (
        set RES_FILE="%BUILD_DIR%\OneClickRGB.res"
    )
)

REM Output
set OUTPUT=%BUILD_DIR%\OneClickRGB.exe

echo Output: %OUTPUT%
echo.

REM /Fo with a trailing backslash keeps the .obj files out of the source tree.
cl.exe %CFLAGS% %INCLUDES% %SOURCE% %RES_FILE% /Fo"%BUILD_DIR%\\" /Fe"%OUTPUT%" /link %LIBS%

if errorlevel 1 (
    echo.
    echo ================================================
    echo BUILD FAILED
    echo ================================================
    exit /b 1
)

REM Copy dependencies
echo.
echo Copying dependencies...
copy /Y "%DEP_DIR%\hidapi\hidapi.dll" "%BUILD_DIR%\" >nul 2>&1
copy /Y "%DEP_DIR%\PawnIO\PawnIOLib.dll" "%BUILD_DIR%\" >nul 2>&1

if not exist "%BUILD_DIR%\modules" mkdir "%BUILD_DIR%\modules"
copy /Y "%DEP_DIR%\PawnIO\modules\*.bin" "%BUILD_DIR%\modules\" >nul 2>&1

if not exist "%BUILD_DIR%\config" mkdir "%BUILD_DIR%\config"
if exist "%ROOT_DIR%\config\*.json" (
    copy /Y "%ROOT_DIR%\config\*.json" "%BUILD_DIR%\config\" >nul 2>&1
)

echo.
echo ================================================
echo BUILD SUCCESSFUL!
echo ================================================
echo.
echo Executable: %OUTPUT%
echo.
echo To run: cd build ^&^& OneClickRGB.exe
echo.

endlocal
exit /b 0
