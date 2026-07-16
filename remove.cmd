@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "WPM_INSTALL_SCOPE="
if /I "%~1"=="--user" (
    set "WPM_INSTALL_SCOPE=user"
    shift
)
if /I "%~1"=="--machine" (
    set "WPM_INSTALL_SCOPE=machine"
    shift
)
if not defined WPM_INSTALL_SCOPE (
    whoami /groups /fo csv /nh 2>nul | findstr /C:"S-1-16-12288" /C:"S-1-16-16384" >nul
    if errorlevel 1 (
        set "WPM_INSTALL_SCOPE=user"
    ) else (
        set "WPM_INSTALL_SCOPE=machine"
    )
)

if /I "%WPM_INSTALL_SCOPE%"=="user" (
    if not defined WPM_INSTALL_DIR set "WPM_INSTALL_DIR=%LocalAppData%\WPM"
    if not defined WPM_DATA_DIR set "WPM_DATA_DIR=%LocalAppData%\WPM\data"
    if not defined WPM_ENVIRONMENT_REGISTRY_KEY set "WPM_ENVIRONMENT_REGISTRY_KEY=HKCU\Environment"
) else if not defined WPM_INSTALL_DIR (
    if defined ProgramW6432 (
        set "WPM_INSTALL_DIR=%ProgramW6432%\WPM"
    ) else (
        set "WPM_INSTALL_DIR=%ProgramFiles%\WPM"
    )
)
if not defined WPM_DATA_DIR set "WPM_DATA_DIR=%ProgramData%\WPM"
if not defined WPM_ENVIRONMENT_REGISTRY_KEY set "WPM_ENVIRONMENT_REGISTRY_KEY=HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment"

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

set "WPM_PATH="
for /f "tokens=1,2,*" %%A in ('reg query "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v Path 2^>nul ^| find /I "Path"') do set "WPM_PATH=%%C"
if defined WPM_PATH (
    set "WPM_PATH=!WPM_PATH:;%%WPM%%=!"
    set "WPM_PATH=!WPM_PATH:%%WPM%%;=!"
    set "WPM_PATH=!WPM_PATH:%%WPM%%=!"
    reg add "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v Path /t REG_EXPAND_SZ /d "!WPM_PATH!" /f >nul
)
reg delete "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v WPM /f >nul 2>nul
if /I "%WPM_INSTALL_SCOPE%"=="user" reg delete "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v WPM_DATA_DIR /f >nul 2>nul

echo WPM removed from "%WPM_INSTALL_DIR%"
echo WPM data removed from "%WPM_DATA_DIR%"
exit /b 0
