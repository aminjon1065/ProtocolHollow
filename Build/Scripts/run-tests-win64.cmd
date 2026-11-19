@echo off
rem cmd.exe / double-click entry point for run-tests-win64.ps1 (execution policy bypassed for this process only).
rem Example: Build\Scripts\run-tests-win64.cmd -Filter Hollow.Core -NoBuild
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-tests-win64.ps1" %*
exit /b %ERRORLEVEL%
