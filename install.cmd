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
    "  New-Item -ItemType Directory -Path $work | Out-Null;" ^
    "  $indexPath = Join-Path $work 'index.json';" ^
    "  Invoke-WebRequest -UseBasicParsing -Uri ($releaseBase + '/index.json') -OutFile $indexPath;" ^
    "  $index = Get-Content -Raw -LiteralPath $indexPath | ConvertFrom-Json;" ^
    "  if ($index.version -ne 1) { throw 'Unsupported WPM repository index schema.' };" ^
    "  $candidates = @($index.packages | Where-Object { $_.name -eq 'wpm' -and $_.arch -eq $arch });" ^
    "  if ($candidates.Count -ne 1) { throw \"Expected one WPM package for $arch; found $($candidates.Count).\" };" ^
    "  $packageVersion = [string]$candidates[0].version;" ^
    "  $asset = [string]$candidates[0].url;" ^
    "  if (-not $asset -or [IO.Path]::GetFileName($asset) -ne $asset -or $asset -notmatch '^wpm-(x86|x64|arm64)-[0-9A-Za-z.-]+\.zip$') { throw 'The release index contains an unsafe WPM package URL.' };" ^
    "  $archive = Join-Path $work $asset;" ^
    "  $publicKey = Join-Path $work 'wpm-release.public';" ^
    "  Write-Host \"Downloading WPM $packageVersion for $arch...\";" ^
    "  Invoke-WebRequest -UseBasicParsing -Uri ($releaseBase + '/' + $asset) -OutFile $archive;" ^
    "  Invoke-WebRequest -UseBasicParsing -Uri ($releaseBase + '/wpm-release.public') -OutFile $publicKey;" ^
    "  $package = Join-Path $work 'package';" ^
    "  Expand-Archive -LiteralPath $archive -DestinationPath $package;" ^
    "  $wpm = Join-Path $package 'wpm.exe';" ^
    "  $setup = Join-Path $package 'setup.cmd';" ^
    "  if (-not (Test-Path -LiteralPath $wpm) -or -not (Test-Path -LiteralPath $setup)) { throw 'The downloaded WPM package is incomplete.' };" ^
    "  $previousData = $env:WPM_DATA_DIR;" ^
    "  try {" ^
    "    $env:WPM_DATA_DIR = Join-Path $work 'validation';" ^
    "    & $wpm trust add $publicKey; if ($LASTEXITCODE -ne 0) { throw 'Could not establish temporary release-key trust.' };" ^
    "    & $wpm verify $archive; if ($LASTEXITCODE -ne 0) { throw 'Downloaded WPM package validation failed.' };" ^
    "  } finally { if ($null -eq $previousData) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue } else { $env:WPM_DATA_DIR = $previousData } };" ^
    "  & $setup $wpm; if ($LASTEXITCODE -ne 0) { throw \"WPM setup failed with exit code $LASTEXITCODE.\" };" ^
    "  Write-Host 'WPM installation completed. Open a new command window and run wpm --version.';" ^
    "} catch { Write-Error $_; exit 1 } finally { if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force } }"

exit /b %errorlevel%
