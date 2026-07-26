@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0TinyCCArchiver.ps1" %*
exit /b %ERRORLEVEL%
