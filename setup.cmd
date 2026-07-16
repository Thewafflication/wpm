@echo off
setlocal EnableExtensions

set "SOURCE_EXE=%~1"
if not defined SOURCE_EXE set "SOURCE_EXE=%~dp0wpm.exe"

if not exist "%SOURCE_EXE%" (
    echo Error: WPM executable not found: "%SOURCE_EXE%"
    exit /b 1
)

if not defined WPM_INSTALL_DIR (
    if defined ProgramW6432 (
        set "WPM_INSTALL_DIR=%ProgramW6432%\WPM"
    ) else (
        set "WPM_INSTALL_DIR=%ProgramFiles%\WPM"
    )
)
if not defined WPM_DATA_DIR set "WPM_DATA_DIR=%ProgramData%\WPM"
if not defined WPM_ENVIRONMENT_REGISTRY_KEY set "WPM_ENVIRONMENT_REGISTRY_KEY=HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment"

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

reg add "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v WPM /t REG_SZ /d "%WPM_INSTALL_DIR%" /f >nul
if errorlevel 1 (
    echo Error: could not set the WPM environment variable. Run setup from an elevated command prompt.
    exit /b 1
)

set "WPM_PATH="
for /f "tokens=1,2,*" %%A in ('reg query "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v Path 2^>nul ^| find /I "Path"') do set "WPM_PATH=%%C"
echo;%WPM_PATH%;| findstr /I /C:";%%WPM%%;" >nul
if errorlevel 1 set "WPM_PATH=%WPM_PATH%;%%WPM%%"
reg add "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v Path /t REG_EXPAND_SZ /d "%WPM_PATH%" /f >nul
if errorlevel 1 (
    echo Error: could not add %%WPM%% to the system Path. Run setup from an elevated command prompt.
    exit /b 1
)

echo WPM installed to "%WPM_INSTALL_DIR%\wpm.exe"
echo WPM data directory: "%WPM_DATA_DIR%"
echo The system WPM variable and Path have been updated. Open a new command prompt to use wpm.
exit /b 0
