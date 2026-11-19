@echo off
rem cmd.exe / double-click entry point for import-placeholders.ps1 (execution policy bypassed for this process only).
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0import-placeholders.ps1" %*
exit /b %ERRORLEVEL%
