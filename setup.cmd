@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "SOURCE_ROOT=%~dp0"

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

set "SOURCE_EXE=%~1"
if not defined SOURCE_EXE set "SOURCE_EXE=%SOURCE_ROOT%wpm.exe"

if not exist "%SOURCE_EXE%" (
    echo Error: WPM executable not found: "%SOURCE_EXE%"
    exit /b 1
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
if not defined WPM_ENVIRONMENT_REGISTRY_KEY set "WPM_ENVIRONMENT_REGISTRY_KEY=HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment"

if not exist "%WPM_INSTALL_DIR%" mkdir "%WPM_INSTALL_DIR%"
if errorlevel 1 (
    echo Error: could not create installation directory: "%WPM_INSTALL_DIR%"
    exit /b 1
)

copy /y "%SOURCE_EXE%" "%WPM_INSTALL_DIR%\wpm.exe" >nul
if errorlevel 1 (
    echo Error: could not install WPM to "%WPM_INSTALL_DIR%"
    exit /b 1
)

for %%F in ("%SOURCE_ROOT%README.md" "%SOURCE_ROOT%LICENSE.txt" "%SOURCE_ROOT%THIRD_PARTY_NOTICES.md") do (
    if not exist "%%~fF" (
        echo Error: required WPM distribution file not found: "%%~fF"
        exit /b 1
    )
    copy /y "%%~fF" "%WPM_INSTALL_DIR%\%%~nxF" >nul
    if errorlevel 1 (
        echo Error: could not install %%~nxF to "%WPM_INSTALL_DIR%"
        exit /b 1
    )
)

if not exist "%SOURCE_ROOT%docs\usage.md" (
    echo Error: required WPM documentation not found: "%SOURCE_ROOT%docs\usage.md"
    exit /b 1
)
if not exist "%WPM_INSTALL_DIR%\docs" mkdir "%WPM_INSTALL_DIR%\docs"
if errorlevel 1 (
    echo Error: could not create documentation directory: "%WPM_INSTALL_DIR%\docs"
    exit /b 1
)
for %%F in ("%SOURCE_ROOT%docs\*.md") do (
    copy /y "%%~fF" "%WPM_INSTALL_DIR%\docs\%%~nxF" >nul
    if errorlevel 1 (
        echo Error: could not install documentation file %%~nxF
        exit /b 1
    )
)

reg add "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v WPM /t REG_SZ /d "%WPM_INSTALL_DIR%" /f >nul
if errorlevel 1 (
    echo Error: could not set the WPM environment variable. Run setup from an elevated command prompt.
    exit /b 1
)

if /I "%WPM_INSTALL_SCOPE%"=="user" (
    reg add "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v WPM_DATA_DIR /t REG_SZ /d "%WPM_DATA_DIR%" /f >nul
    if errorlevel 1 (
        echo Error: could not set the WPM user data directory.
        exit /b 1
    )
)

set "WPM_PATH="
for /f "tokens=1,2,*" %%A in ('reg query "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v Path 2^>nul ^| find /I "Path"') do set "WPM_PATH=%%C"
echo;!WPM_PATH!;| findstr /I /C:"WPM" >nul
if errorlevel 1 set "WPM_PATH=!WPM_PATH!;%%WPM%%"
reg add "%WPM_ENVIRONMENT_REGISTRY_KEY%" /v Path /t REG_EXPAND_SZ /d "!WPM_PATH!" /f >nul
if errorlevel 1 (
    echo Error: could not add %%WPM%% to the system Path. Run setup from an elevated command prompt.
    exit /b 1
)

echo WPM executable, licenses, and documentation installed to "%WPM_INSTALL_DIR%"
if /I "%WPM_INSTALL_SCOPE%"=="user" (
    echo The user WPM variable and Path have been updated. Open a new command prompt to use wpm.
) else (
    echo The system WPM variable and Path have been updated. Open a new command prompt to use wpm.
)
exit /b 0
