param(
    [Parameter(Mandatory = $true)]
    [string]$PreviousVersion,

    [Parameter(Mandatory = $true)]
    [string]$CandidateVersion,

    [Parameter(Mandatory = $true)]
    [ValidateSet('x86', 'x64', 'arm64')]
    [string]$Architecture,

    [Parameter(Mandatory = $true)]
    [string]$CandidatePackage,

    [Parameter(Mandatory = $true)]
    [string]$PublicKey
)

$ErrorActionPreference = 'Stop'
$CandidatePackage = (Resolve-Path -LiteralPath $CandidatePackage).Path
$PublicKey = (Resolve-Path -LiteralPath $PublicKey).Path
$testId = [Guid]::NewGuid().ToString('N')
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-release-upgrade-$testId"
$previousPackageName = "wpm-$Architecture-$PreviousVersion.zip"
$candidatePackageName = "wpm-$Architecture-$CandidateVersion.zip"
$previousPackage = Join-Path $testRoot $previousPackageName
$previousRoot = Join-Path $testRoot 'previous'
$dataRoot = Join-Path $testRoot 'data'
$installRoot = Join-Path $testRoot 'install'
$registryKey = "HKCU\Software\WPM\ReleaseUpgradeTests\$testId"
$repositoryUrl = 'https://github.com/Thewafflication/wpm/releases/latest/download'
$previousDataRoot = $env:WPM_DATA_DIR
$previousInstallRoot = $env:WPM_INSTALL_DIR
$previousRegistryKey = $env:WPM_ENVIRONMENT_REGISTRY_KEY

function Get-RepositoryCachePath {
    param([string]$DataDir, [string]$Url)
    [uint32]$hash = 2166136261
    foreach ($byte in [Text.Encoding]::UTF8.GetBytes($Url)) {
        $hash = $hash -bxor $byte
        $hash = [uint32](([uint64]$hash * [uint64]16777619) % [uint64]4294967296)
    }
    Join-Path $DataDir ("cache\repositories\{0:x8}.json" -f $hash)
}

try {
    New-Item -ItemType Directory -Force -Path $testRoot, $previousRoot,
        (Join-Path $dataRoot 'packages'), (Join-Path $dataRoot 'cache\packages') | Out-Null
    Invoke-WebRequest `
        -Uri "https://github.com/Thewafflication/wpm/releases/download/$PreviousVersion/$previousPackageName" `
        -OutFile $previousPackage
    Expand-Archive -LiteralPath $previousPackage -DestinationPath $previousRoot

    $previousExecutable = Join-Path $previousRoot 'wpm.exe'
    if (-not (Test-Path -LiteralPath $previousExecutable -PathType Leaf)) {
        throw "Previous release package does not contain wpm.exe: $previousPackageName"
    }

    Copy-Item -LiteralPath $previousPackage -Destination (Join-Path $dataRoot "packages\$previousPackageName")
    Copy-Item -LiteralPath $CandidatePackage -Destination (Join-Path $dataRoot "cache\packages\$candidatePackageName")
    $indexPath = Get-RepositoryCachePath -DataDir $dataRoot -Url $repositoryUrl
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $indexPath) | Out-Null
    [ordered]@{
        version = 1
        packages = @([ordered]@{
            name = 'wpm'
            version = $CandidateVersion
            arch = $Architecture
            url = $candidatePackageName
        })
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $indexPath -Encoding utf8

    $env:WPM_DATA_DIR = $dataRoot
    $env:WPM_INSTALL_DIR = $installRoot
    $env:WPM_ENVIRONMENT_REGISTRY_KEY = $registryKey
    & $previousExecutable trust add $PublicKey
    if ($LASTEXITCODE -ne 0) { throw 'The previous release could not trust the candidate release key.' }

    $output = & $previousExecutable upgrade wpm --arch $Architecture --version $CandidateVersion --offline 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or $output -notmatch '(?i)scheduled') {
        throw "The previous release could not schedule the candidate upgrade. $output"
    }

    $logPath = Join-Path $dataRoot "audit\self-upgrade-$Architecture-$CandidateVersion.log"
    $installedExecutable = Join-Path $installRoot 'wpm.exe'
    $deadline = [DateTime]::UtcNow.AddSeconds(90)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ((Test-Path -LiteralPath $logPath) -and
            (Get-Content -Raw -LiteralPath $logPath) -match "Result: wpm $Architecture upgraded") { break }
        Start-Sleep -Milliseconds 200
    }
    if (-not (Test-Path -LiteralPath $logPath)) {
        throw 'The previous release scheduled the handoff, but the candidate never started.'
    }
    $log = Get-Content -Raw -LiteralPath $logPath
    if ($log -notmatch "Result: wpm $Architecture upgraded") {
        throw "The candidate handoff did not complete the upgrade. $log"
    }
    if (-not (Test-Path -LiteralPath $installedExecutable -PathType Leaf)) {
        throw 'The upgrade did not install the candidate executable.'
    }
    $versionOutput = & $installedExecutable --version 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or $versionOutput -notmatch "Version\s+$([regex]::Escape($CandidateVersion))") {
        throw "The installed candidate is not usable or has the wrong version. $versionOutput"
    }
    Write-Host "Verified WPM upgrade: $PreviousVersion -> $CandidateVersion ($Architecture)."
}
finally {
    $env:WPM_DATA_DIR = $previousDataRoot
    $env:WPM_INSTALL_DIR = $previousInstallRoot
    $env:WPM_ENVIRONMENT_REGISTRY_KEY = $previousRegistryKey
    & reg.exe delete $registryKey /f 2>$null | Out-Null
}
