@echo off
setlocal
cd /d "%~dp0"
cmd /c "%~dp0test_edge_only.bat" %*
set EXIT_CODE=%ERRORLEVEL%
echo.
echo run_test_edge_only.cmd finished with exit code %EXIT_CODE%
pause
exit /b %EXIT_CODE%
