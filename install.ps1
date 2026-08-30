param(
    [switch]$User
)

# Keep this bootstrap compatible with Windows PowerShell 2.0 and the cmdlets
# and language syntax shipped with Windows XP.
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$releaseBase = 'https://github.com/Thewafflication/wpm/releases/latest/download'
$work = Join-Path ([IO.Path]::GetTempPath()) ('wpm-install-' + [Guid]::NewGuid().ToString('N'))

function Invoke-WpmDownload {
    param(
        [string]$Url,
        [string]$Destination
    )

    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($curl -ne $null) {
        & $curl.Path --fail --location --show-error --silent --tlsv1.2 --output $Destination $Url
        if ($LASTEXITCODE -ne 0) {
            throw "curl.exe could not download $Url (exit code $LASTEXITCODE)."
        }
        return
    }

    # 3072 is TLS 1.2. The numeric value works on PowerShell 2.0, whose enum
    # does not name TLS 1.2. The OS/.NET HTTPS provider must still support it.
    try { [Net.ServicePointManager]::SecurityProtocol = 3072 } catch { }
    $client = New-Object Net.WebClient
    try {
        $client.Headers.Add('User-Agent', 'WPM-PowerShell-2-Bootstrap')
        $client.DownloadFile($Url, $Destination)
    }
    catch {
        throw ('Could not download {0}. On Windows XP, install a TLS 1.2-capable ' +
            'curl.exe or download install.ps1 and the release assets using another machine. {1}' -f
            $Url, $_.Exception.Message)
    }
    finally {
        $client.Dispose()
    }
}

function Expand-WpmZip {
    param(
        [string]$Archive,
        [string]$Destination
    )

    New-Item -ItemType Directory -Path $Destination | Out-Null
    $shell = New-Object -ComObject Shell.Application
    $source = $shell.NameSpace($Archive)
    $target = $shell.NameSpace($Destination)
    if ($source -eq $null -or $target -eq $null) {
        throw 'Windows Explorer could not open the downloaded WPM package.'
    }

    # FOF_SILENT (4) plus FOF_NOCONFIRMATION (16).
    $target.CopyHere($source.Items(), 20)
    $requiredFiles = @(
        'wpm.exe',
        'setup.cmd',
        'README.md',
        'LICENSE.txt',
        'THIRD_PARTY_NOTICES.md',
        'docs\usage.md'
    )
    $deadline = [DateTime]::UtcNow.AddMinutes(2)
    $previousState = ''
    $stableChecks = 0
    while ([DateTime]::UtcNow -lt $deadline) {
        $complete = $true
        foreach ($requiredFile in $requiredFiles) {
            if (-not (Test-Path (Join-Path $Destination $requiredFile))) {
                $complete = $false
                break
            }
        }

        $files = @(Get-ChildItem $Destination -Recurse -Force | Where-Object { -not $_.PSIsContainer })
        [Int64]$totalLength = 0
        foreach ($file in $files) { $totalLength += $file.Length }
        $state = [string]$files.Count + ':' + [string]$totalLength
        if ($complete -and $state -eq $previousState) { $stableChecks++ }
        else { $stableChecks = 0 }
        if ($stableChecks -ge 5) { return }
        $previousState = $state
        Start-Sleep -Milliseconds 200
    }
    throw 'Timed out while extracting the WPM package.'
}

$hadDataDirectory = Test-Path Env:WPM_DATA_DIR
$previousDataDirectory = $env:WPM_DATA_DIR

try {
    New-Item -ItemType Directory -Path $work | Out-Null

    $native = $env:PROCESSOR_ARCHITECTURE
    if ($env:PROCESSOR_ARCHITEW6432) { $native = $env:PROCESSOR_ARCHITEW6432 }
    switch ($native.ToUpperInvariant()) {
        'AMD64' { $architecture = 'x64' }
        'X86' { $architecture = 'x86' }
        'ARM64' { $architecture = 'arm64' }
        default { throw "Unsupported Windows architecture: $native" }
    }
    Write-Host "Detected Windows architecture: $architecture."

    $indexPath = Join-Path $work 'index.json'
    Write-Host 'Downloading the latest WPM release index...'
    Invoke-WpmDownload ($releaseBase + '/index.json') $indexPath
    $indexText = [IO.File]::ReadAllText($indexPath)
    if ($indexText -notmatch '"version"\s*:\s*1') {
        throw 'Unsupported WPM repository index schema.'
    }

    $architecturePattern = [Regex]::Escape($architecture)
    $packagePattern = '(?s)\{[^{}]*"name"\s*:\s*"wpm"[^{}]*"version"\s*:\s*"([^"]+)"[^{}]*"arch"\s*:\s*"' +
        $architecturePattern + '"[^{}]*"url"\s*:\s*"([^"]+)"[^{}]*\}'
    $packageMatches = [Regex]::Matches($indexText, $packagePattern)
    if ($packageMatches.Count -ne 1) {
        throw "Expected one WPM package for $architecture; found $($packageMatches.Count)."
    }

    $packageVersion = $packageMatches[0].Groups[1].Value
    $asset = $packageMatches[0].Groups[2].Value
    if ([IO.Path]::GetFileName($asset) -ne $asset -or
        $asset -notmatch '^wpm-(x86|x64|arm64)-[0-9A-Za-z.-]+\.zip$') {
        throw 'The release index contains an unsafe WPM package URL.'
    }
    Write-Host "Selected WPM $packageVersion for $architecture."

    $archive = Join-Path $work $asset
    $publicKey = Join-Path $work 'wpm-release.public'
    Write-Host 'Downloading the WPM package...'
    Invoke-WpmDownload ($releaseBase + '/' + $asset) $archive
    Write-Host 'Downloading the WPM release signing key...'
    Invoke-WpmDownload ($releaseBase + '/wpm-release.public') $publicKey

    $package = Join-Path $work 'package'
    Write-Host 'Extracting the WPM package...'
    Expand-WpmZip $archive $package
    $wpm = Join-Path $package 'wpm.exe'
    $setup = Join-Path $package 'setup.cmd'

    $env:WPM_DATA_DIR = Join-Path $work 'validation'
    Write-Host 'Establishing temporary trust in the release signing key...'
    & $wpm trust add $publicKey
    if ($LASTEXITCODE -ne 0) { throw 'Could not establish temporary release-key trust.' }
    Write-Host 'Verifying the WPM package signature and contents...'
    & $wpm verify $archive
    if ($LASTEXITCODE -ne 0) { throw 'Downloaded WPM package validation failed.' }

    if ($hadDataDirectory) { $env:WPM_DATA_DIR = $previousDataDirectory }
    else { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue }

    $scope = '--machine'
    if ($User) { $scope = '--user' }
    Write-Host "Starting packaged WPM setup ($scope)..."
    & $setup $scope $wpm
    if ($LASTEXITCODE -ne 0) { throw "WPM setup failed with exit code $LASTEXITCODE." }
    Write-Host 'WPM installation completed. Open a new command window and run wpm --version.'
}
finally {
    if ($hadDataDirectory) { $env:WPM_DATA_DIR = $previousDataDirectory }
    else { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue }
    if (Test-Path $work) { Remove-Item $work -Recurse -Force }
}
