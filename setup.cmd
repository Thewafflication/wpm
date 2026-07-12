@echo off
setlocal EnableExtensions

set "SOURCE_EXE=%~1"
if not defined SOURCE_EXE set "SOURCE_EXE=%~dp0wpm.exe"

if not exist "%SOURCE_EXE%" (
    echo Error: WPM executable not found: "%SOURCE_EXE%"
    exit /b 1
)

if not defined WPM_INSTALL_DIR set "WPM_INSTALL_DIR=%ProgramFiles%\WPM"
if not defined WPM_DATA_DIR set "WPM_DATA_DIR=%ProgramData%\WPM"

if not exist "%WPM_INSTALL_DIR%" mkdir "%WPM_INSTALL_DIR%"
if errorlevel 1 (
    echo Error: could not create installation directory: "%WPM_INSTALL_DIR%"
    exit /b 1
)

if not exist "%WPM_DATA_DIR%" mkdir "%WPM_DATA_DIR%"
if errorlevel 1 (
    echo Error: could not create data directory: "%WPM_DATA_DIR%"
    exit /b 1
)

copy /y "%SOURCE_EXE%" "%WPM_INSTALL_DIR%\wpm.exe" >nul
if errorlevel 1 (
    echo Error: could not install WPM to "%WPM_INSTALL_DIR%"
    exit /b 1
)

echo WPM installed to "%WPM_INSTALL_DIR%\wpm.exe"
echo WPM data directory: "%WPM_DATA_DIR%"
exit /b 0
