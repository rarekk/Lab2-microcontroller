@echo off
REM Wrapper so the build works even when PowerShell script execution is restricted.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_all.ps1"
exit /b %ERRORLEVEL%
