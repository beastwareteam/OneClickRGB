@echo off
setlocal enabledelayedexpansion

echo ================================================
echo OneClickRGB - Build Script
echo ================================================

:: Setup Visual Studio environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo ERROR: Could not setup Visual Studio environment
    exit /b 1
)

:: Set paths
set SRC_DIR=%~dp0..\src
set BUILD_DIR=%~dp0build
set ROOT_DIR=%~dp0..
set QT_DIR=C:\Qt\5.15.0\msvc2019_64

:: Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo Compiling OneClickRGB...
echo.

:: Compile all source files
set SOURCES=
set SOURCES=%SOURCES% "%SRC_DIR%\main_gui.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\ui\MainWindow.cpp"
set SOURCES=%SOURCES% "%BUILD_DIR%\moc_MainWindow.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\core\OneClickRGB.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\core\DeviceManager.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\core\ProfileManager.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\core\ConfigManager.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\core\AutoStart.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\core\DeviceRegistry.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\core\modules\ModuleManager.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\scanner\HardwareScanner.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\devices\RGBDevice.cpp"
set SOURCES=%SOURCES% "%SRC_DIR%\devices\HIDController.cpp"

:: Compiler flags
set CFLAGS=/nologo /EHsc /std:c++17 /O2 /MD /W3
set CFLAGS=%CFLAGS% /DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE

:: Include paths
set INCLUDES=/I"%SRC_DIR%"
set INCLUDES=%INCLUDES% /I"%ROOT_DIR%\dependencies\hidapi"
set INCLUDES=%INCLUDES% /I"%ROOT_DIR%\dependencies"
set INCLUDES=%INCLUDES% /I"%QT_DIR%\include"
set INCLUDES=%INCLUDES% /I"%QT_DIR%\include\QtCore"
set INCLUDES=%INCLUDES% /I"%QT_DIR%\include\QtGui"
set INCLUDES=%INCLUDES% /I"%QT_DIR%\include\QtWidgets"

:: Libraries
set LIBS=shell32.lib advapi32.lib setupapi.lib "%ROOT_DIR%\dependencies\hidapi\hidapi.lib" Qt5Core.lib Qt5Gui.lib Qt5Widgets.lib

:: Library paths
set LIBPATHS=/LIBPATH:"%QT_DIR%\lib"

:: Output
set OUTPUT=%BUILD_DIR%\oneclickrgb.exe

echo Compiler: cl.exe
echo Output: %OUTPUT%
echo Includes: %INCLUDES%
echo.

cl.exe %CFLAGS% %INCLUDES% %SOURCES% /Fe"%OUTPUT%" /link %LIBPATHS% %LIBS%

if errorlevel 1 (
    echo.
    echo ================================================
    echo BUILD FAILED
    echo ================================================
    exit /b 1
)

echo.
echo ================================================
echo BUILD SUCCESSFUL!
echo ================================================
echo.
echo Executable: %OUTPUT%
echo.
echo Run with: %OUTPUT%
echo.

endlocal
