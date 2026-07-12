@echo off
setlocal EnableExtensions

if not defined WPM_INSTALL_DIR set "WPM_INSTALL_DIR=%ProgramFiles%\WPM"
if not defined WPM_DATA_DIR set "WPM_DATA_DIR=%ProgramData%\WPM"

if exist "%WPM_INSTALL_DIR%" (
    rmdir /s /q "%WPM_INSTALL_DIR%"
    if errorlevel 1 (
        echo Error: could not remove WPM installation directory: "%WPM_INSTALL_DIR%"
        exit /b 1
    )
)

if exist "%WPM_DATA_DIR%" (
    rmdir /s /q "%WPM_DATA_DIR%"
    if errorlevel 1 (
        echo Error: could not remove WPM data directory: "%WPM_DATA_DIR%"
        exit /b 1
    )
)

echo WPM removed from "%WPM_INSTALL_DIR%"
echo WPM data removed from "%WPM_DATA_DIR%"
exit /b 0
