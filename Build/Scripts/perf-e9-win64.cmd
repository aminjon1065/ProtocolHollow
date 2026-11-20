@echo off
rem cmd.exe / double-click entry point for perf-e9-win64.ps1 (execution policy bypassed for this process only).
rem Example: Build\Scripts\perf-e9-win64.cmd -SkipPackage -ClientTrace
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0perf-e9-win64.ps1" %*
exit /b %ERRORLEVEL%
