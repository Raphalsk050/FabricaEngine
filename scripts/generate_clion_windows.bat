@echo off
setlocal

set SCRIPT_DIR=%~dp0
powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%generate_clion_windows.ps1" %*
exit /b %ERRORLEVEL%