param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,

    [string]$EvidenceTex,

    [switch]$NoFailOnFailure
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

function Get-RepositoryCachePath {
    param([string]$DataDir, [string]$Url)

    [uint32]$hash = 2166136261
    foreach ($byte in [Text.Encoding]::UTF8.GetBytes($Url)) {
        $hash = $hash -bxor $byte
        $hash = [uint32](([uint64]$hash * [uint64]16777619) % [uint64]4294967296)
    }
    return Join-Path $DataDir ("cache\repositories\{0:x8}.json" -f $hash)
}

$testId = [Guid]::NewGuid().ToString('N')
$packageName = "repository-test-$testId"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-repositories-$testId"
$dataDir = Join-Path $testRoot 'wpm-data'
$sourceA = Join-Path $testRoot 'source-a'
$sourceB = Join-Path $testRoot 'source-b'
$outputA = Join-Path $testRoot 'output-a'
$outputB = Join-Path $testRoot 'output-b'
$deployment = Join-Path $testRoot 'deployment.txt'
$previousDataDir = $env:WPM_DATA_DIR
$repositoryA = 'https://repo-a.example.test'
$repositoryB = 'https://repo-b.example.test'
$unavailableRepository = 'https://unavailable.example.test'
$started = Get-Date
$results = @()

function New-TestPackage {
    param([string]$Source, [string]$Marker)
    New-Item -ItemType Directory -Force -Path (Join-Path $Source '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $Source '.wpm\package.txt') -Value @(
        "name=$packageName"
        'version=1.0.0'
        'arch=any'
        'debug=false'
    )
    Set-Content -LiteralPath (Join-Path $Source '.wpm\install.cmd') -Value @(
        '@echo off'
        "echo $Marker > `"$deployment`""
    )
}

try {
    $env:WPM_DATA_DIR = $dataDir
    New-Item -ItemType Directory -Force -Path $sourceA, $sourceB, $outputA, $outputB | Out-Null
    New-TestPackage -Source $sourceA -Marker 'repository-a'
    New-TestPackage -Source $sourceB -Marker 'repository-b'

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'List built-in GitHub release repository' -Arguments @('repo', 'list') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch 'github\.com/Thewafflication/wpm/releases/latest/download') { throw 'Built-in GitHub release repository was not listed.' }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build repository A package' -Arguments @('build', $sourceA, $outputA, '--no-index') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Expected exit code 0, got $ExitCode." }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build repository B package' -Arguments @('build', $sourceB, $outputB, '--no-index') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Expected exit code 0, got $ExitCode." }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Configure repositories with priority' -Arguments @('repo', 'add', $repositoryA) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Expected exit code 0, got $ExitCode." }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Configure higher-priority repository' -Arguments @('repo', 'add', $repositoryB, '--priority', '5') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Expected exit code 0, got $ExitCode." }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Configure unavailable repository' -Arguments @('repo', 'add', $unavailableRepository) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Expected exit code 0, got $ExitCode." }
    }
    $results += New-WpmManualStep -Name 'Seed controlled cached indexes and package' -Action {
        $archiveName = "$packageName-any-1.0.0.zip"
        $cachePackages = Join-Path $dataDir 'cache\packages'
        New-Item -ItemType Directory -Force -Path $cachePackages | Out-Null
        Copy-Item -LiteralPath (Join-Path $outputB $archiveName) -Destination (Join-Path $cachePackages "$packageName-1.0.0-any.zip")
        foreach ($repository in @($repositoryA, $repositoryB)) {
            $path = Get-RepositoryCachePath $dataDir $repository
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
            Set-Content -LiteralPath $path -NoNewline -Value "{`"version`":1,`"packages`": [{`"name`":`"$packageName`",`"version`":`"2.0.0`",`"arch`":`"x64`",`"url`":`"packages/$archiveName`"},{`"name`":`"$packageName`",`"version`":`"1.0.0`",`"arch`":`"any`",`"url`":`"packages/$archiveName`"}]}"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Refresh available cached indexes despite unavailable repositories' -Arguments @('repo', 'update', '--offline') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Expected available cached repositories to refresh successfully, got exit code $ExitCode. $Output" }
        if ($Output -notmatch 'no cached index') { throw 'Expected the unavailable repository to be reported during offline refresh.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'List configured repositories' -Arguments @('repo', 'list') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch [regex]::Escape($repositoryB) -or $Output -notmatch "5\s+$([regex]::Escape($repositoryB))") { throw 'Repository list did not show configured priorities.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Install named package from offline cache using priority' -Arguments @('install', $packageName, '--offline', '--allow-unsigned') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Expected exit code 0, got $ExitCode. $Output" }
        if ((Get-Content -Raw -LiteralPath $deployment).Trim() -ne 'repository-b') { throw 'Package priority did not select repository B.' }
        if ($Output -notmatch [regex]::Escape($repositoryB)) { throw 'Expected selected repository in install output.' }
        if ($Output -notmatch 'Installing .+ 1\.0\.0 ') { throw 'Resolver selected the incompatible x64 package.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject uncached named package while offline' -Arguments @('install', 'missing-package', '--offline') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'was not found') { throw 'Expected offline resolution failure for missing package.' }
    }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) { Write-WpmTestEvidence -TestCaseId 'TC-0011' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex }
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
    if ($null -eq $previousDataDir) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue } else { $env:WPM_DATA_DIR = $previousDataDir }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
