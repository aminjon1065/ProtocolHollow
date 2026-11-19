@echo off
rem cmd.exe / double-click entry point for package-win64.ps1 (execution policy bypassed for this process only).
rem Example: Build\Scripts\package-win64.cmd -Configuration Shipping
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0package-win64.ps1" %*
exit /b %ERRORLEVEL%
