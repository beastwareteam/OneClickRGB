@echo off
REM OneClickRGB Build Script for Windows
REM Prerequisites: CMake 3.16+, Qt 5/6, Visual Studio Build Tools

setlocal EnableDelayedExpansion

echo ========================================
echo OneClickRGB Build Script
echo ========================================
echo.

REM Check for CMake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake not found in PATH
    echo Please install CMake from https://cmake.org/download/
    exit /b 1
)

REM Check for Qt (common locations)
set QT_FOUND=0
for %%Q in (
    "C:\Qt\6.6.0\msvc2019_64"
    "C:\Qt\6.5.0\msvc2019_64"
    "C:\Qt\5.15.2\msvc2019_64"
    "%QTDIR%"
) do (
    if exist "%%~Q\bin\qmake.exe" (
        set "CMAKE_PREFIX_PATH=%%~Q"
        set QT_FOUND=1
        echo Found Qt at: %%~Q
        goto :qt_found
    )
)

if %QT_FOUND%==0 (
    echo WARNING: Qt not found automatically.
    echo Set CMAKE_PREFIX_PATH or QTDIR environment variable to your Qt installation.
    echo Example: set CMAKE_PREFIX_PATH=C:\Qt\6.6.0\msvc2019_64
    echo.
    echo Continuing without Qt - CLI only will be built...
)
:qt_found

REM Create build directory
set BUILD_DIR=%~dp0..\build
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

echo.
echo Configuring with CMake...
echo.

cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%" ^
    -DBUILD_GUI=ON ^
    -DBUILD_CLI=ON

if %ERRORLEVEL% neq 0 (
    echo.
    echo CMake configuration failed!
    exit /b 1
)

echo.
echo Building Release...
echo.

cmake --build . --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo.
    echo Build failed!
    exit /b 1
)

echo.
echo ========================================
echo Build complete!
echo Output: %BUILD_DIR%\Release\
echo ========================================

REM Copy dependencies to Release folder
echo.
echo Copying dependencies...

set RELEASE_DIR=%BUILD_DIR%\Release
copy /Y "%~dp0..\dependencies\hidapi\hidapi.dll" "%RELEASE_DIR%\" >nul 2>&1
copy /Y "%~dp0..\dependencies\PawnIO\PawnIOLib.dll" "%RELEASE_DIR%\" >nul 2>&1

if not exist "%RELEASE_DIR%\modules" mkdir "%RELEASE_DIR%\modules"
copy /Y "%~dp0..\dependencies\PawnIO\modules\*.bin" "%RELEASE_DIR%\modules\" >nul 2>&1

if not exist "%RELEASE_DIR%\config" mkdir "%RELEASE_DIR%\config"
copy /Y "%~dp0..\config\*.json" "%RELEASE_DIR%\config\" >nul 2>&1

echo Dependencies copied.

REM Deploy Qt DLLs if windeployqt is available
if defined CMAKE_PREFIX_PATH (
    if exist "%CMAKE_PREFIX_PATH%\bin\windeployqt.exe" (
        echo.
        echo Deploying Qt dependencies...
        "%CMAKE_PREFIX_PATH%\bin\windeployqt.exe" --release --no-translations --no-opengl-sw "%RELEASE_DIR%\OneClickRGB.exe"
    )
)

echo.
echo Done! Run: %RELEASE_DIR%\OneClickRGB.exe
pause
