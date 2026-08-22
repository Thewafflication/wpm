@echo off
setlocal

if /i "%~1"=="--help" (
    echo Downloads, validates, and installs the latest WPM release for this Windows architecture.
    exit /b 0
)

where powershell.exe >nul 2>&1
if errorlevel 1 (
    echo Error: Windows PowerShell is required to install WPM.
    exit /b 1
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference = 'Stop';" ^
    "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;" ^
    "$releaseBase = 'https://github.com/Thewafflication/wpm/releases/latest/download';" ^
    "$native = if ($env:PROCESSOR_ARCHITEW6432) { $env:PROCESSOR_ARCHITEW6432 } else { $env:PROCESSOR_ARCHITECTURE };" ^
    "$arch = switch ($native.ToUpperInvariant()) { 'AMD64' { 'x64' } 'X86' { 'x86' } 'ARM64' { 'arm64' } default { throw \"Unsupported Windows architecture: $native\" } };" ^
    "$work = Join-Path ([IO.Path]::GetTempPath()) ('wpm-install-' + [Guid]::NewGuid().ToString('N'));" ^
    "try {" ^
    "  Write-Host 'Preparing temporary installation workspace...';" ^
    "  New-Item -ItemType Directory -Path $work | Out-Null;" ^
    "  Write-Host \"Detected Windows architecture: $arch.\";" ^
    "  $indexPath = Join-Path $work 'index.json';" ^
    "  Write-Host 'Checking the latest WPM release...';" ^
    "  Write-Host 'Downloading WPM release index...';" ^
    "  Invoke-WebRequest -UseBasicParsing -Uri ($releaseBase + '/index.json') -OutFile $indexPath;" ^
    "  Write-Host 'Reading WPM release index...';" ^
    "  $index = Get-Content -Raw -LiteralPath $indexPath | ConvertFrom-Json;" ^
    "  if ($index.version -ne 1) { throw 'Unsupported WPM repository index schema.' };" ^
    "  $candidates = @($index.packages | Where-Object { $_.name -eq 'wpm' -and $_.arch -eq $arch });" ^
    "  if ($candidates.Count -ne 1) { throw \"Expected one WPM package for $arch; found $($candidates.Count).\" };" ^
    "  $packageVersion = [string]$candidates[0].version;" ^
    "  $asset = [string]$candidates[0].url;" ^
    "  if (-not $asset -or [IO.Path]::GetFileName($asset) -ne $asset -or $asset -notmatch '^wpm-(x86|x64|arm64)-[0-9A-Za-z.-]+\.zip$') { throw 'The release index contains an unsafe WPM package URL.' };" ^
    "  Write-Host \"Selected WPM $packageVersion for $arch.\";" ^
    "  $archive = Join-Path $work $asset;" ^
    "  $publicKey = Join-Path $work 'wpm-release.public';" ^
    "  Write-Host \"Downloading WPM $packageVersion for $arch...\";" ^
    "  Invoke-WebRequest -UseBasicParsing -Uri ($releaseBase + '/' + $asset) -OutFile $archive;" ^
    "  Write-Host 'Downloading WPM release signing key...';" ^
    "  Invoke-WebRequest -UseBasicParsing -Uri ($releaseBase + '/wpm-release.public') -OutFile $publicKey;" ^
    "  $package = Join-Path $work 'package';" ^
    "  Write-Host 'Extracting WPM package...';" ^
    "  Expand-Archive -LiteralPath $archive -DestinationPath $package;" ^
    "  $wpm = Join-Path $package 'wpm.exe';" ^
    "  $setup = Join-Path $package 'setup.cmd';" ^
    "  Write-Host 'Checking extracted WPM package contents...';" ^
    "  if (-not (Test-Path -LiteralPath $wpm) -or -not (Test-Path -LiteralPath $setup)) { throw 'The downloaded WPM package is incomplete.' };" ^
    "  $previousData = $env:WPM_DATA_DIR;" ^
    "  try {" ^
    "    $env:WPM_DATA_DIR = Join-Path $work 'validation';" ^
    "    Write-Host 'Validating WPM package...';" ^
    "    Write-Host 'Establishing temporary trust in the release signing key...';" ^
    "    & $wpm trust add $publicKey; if ($LASTEXITCODE -ne 0) { throw 'Could not establish temporary release-key trust.' };" ^
    "    Write-Host 'Verifying WPM package signature and contents...';" ^
    "    & $wpm verify $archive; if ($LASTEXITCODE -ne 0) { throw 'Downloaded WPM package validation failed.' };" ^
    "  } finally { if ($null -eq $previousData) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue } else { $env:WPM_DATA_DIR = $previousData } };" ^
    "  Write-Host 'Installing WPM...';" ^
    "  Write-Host 'Starting packaged WPM setup...';" ^
    "  & $setup $wpm; if ($LASTEXITCODE -ne 0) { throw \"WPM setup failed with exit code $LASTEXITCODE.\" };" ^
    "  Write-Host 'WPM installation completed. Open a new command window and run wpm --version.';" ^
    "} catch { Write-Error $_; exit 1 } finally { if (Test-Path -LiteralPath $work) { Write-Host 'Cleaning up temporary installation files...'; Remove-Item -LiteralPath $work -Recurse -Force } }"

exit /b %errorlevel%
