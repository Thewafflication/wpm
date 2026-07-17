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
    Join-Path $DataDir ("cache\repositories\{0:x8}.json" -f $hash)
}

$testId = [Guid]::NewGuid().ToString('N')
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-upgrades-$testId"
$dataDir = Join-Path $testRoot 'wpm-data'
$sourceRoot = Join-Path $testRoot 'sources'
$outputDir = Join-Path $testRoot 'archives'
$markerDir = Join-Path $testRoot 'markers'
$repository = 'https://upgrade.example.test'
$previousDataDir = $env:WPM_DATA_DIR
$started = Get-Date
$results = @()
$entries = [Collections.Generic.List[object]]::new()
$packages = @{
    Architecture = "architecture-$testId"
    Prerelease = "prerelease-$testId"
    Selector = "selector-$testId"
    Conflict = "conflict-$testId"
    Failure = "failure-$testId"
    Continue = "continue-$testId"
    Named = "named-$testId"
    Self = 'wpm'
}

function New-PackageArchive {
    param(
        [string]$Name,
        [string]$Version,
        [string]$Arch,
        [string]$Marker,
        [int]$ExitCode = 0,
        [switch]$IncludeWpmExe,
        [switch]$RepositoryEntry
    )
    $safe = "$Name-$Arch-$($Version.Replace('+','_'))"
    $source = Join-Path $sourceRoot $safe
    New-Item -ItemType Directory -Force -Path (Join-Path $source '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $source '.wpm\package.txt') -Value @(
        "name=$Name", "version=$Version", "arch=$Arch", 'debug=false'
    )
    Set-Content -LiteralPath (Join-Path $source '.wpm\install.cmd') -Value @(
        '@echo off'
        "echo $Marker>>`"$(Join-Path $markerDir "$Name-$Arch.txt")`""
        "exit /b $ExitCode"
    )
    if ($IncludeWpmExe) { Copy-Item -LiteralPath $WpmExe -Destination (Join-Path $source 'wpm.exe') }
    $build = & $WpmExe build $source $outputDir 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Could not build $Name $Arch $Version. $($build -join "`n")" }
    $archive = Join-Path $outputDir "$Name-$Arch-$Version.zip"
    if (-not (Test-Path -LiteralPath $archive)) { throw "Expected archive was not created: $archive" }
    if ($RepositoryEntry) {
        $cacheName = "$Name-$Arch-$Version.zip"
        $cacheDir = Join-Path $dataDir 'cache\packages'
        New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null
        Copy-Item -LiteralPath $archive -Destination (Join-Path $cacheDir $cacheName) -Force
        $entries.Add([ordered]@{ name=$Name; version=$Version; arch=$Arch; url="packages/$cacheName" })
    }
    $archive
}

function Install-Baseline {
    param([string]$Name, [string]$Version, [string]$Arch)
    $archive = New-PackageArchive -Name $Name -Version $Version -Arch $Arch -Marker "baseline-$Version"
    $output = & $WpmExe install $archive --allow-unsigned 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Could not install baseline $Name $Arch $Version. $($output -join "`n")" }
}

try {
    $env:WPM_DATA_DIR = $dataDir
    New-Item -ItemType Directory -Force -Path $sourceRoot, $outputDir, $markerDir | Out-Null

    $results += New-WpmManualStep -Name 'Create installed baselines and repository candidates' -Action {
        Install-Baseline $packages.Architecture '1.0.0' 'x86'
        Install-Baseline $packages.Architecture '1.0.0' 'x64'
        Install-Baseline $packages.Prerelease '1.0.0' 'any'
        Install-Baseline $packages.Selector '1.0.0' 'x86'
        Install-Baseline $packages.Selector '1.0.0' 'x64'
        Install-Baseline $packages.Conflict '1.0.0' 'x86'
        Install-Baseline $packages.Failure '1.0.0' 'any'
        Install-Baseline $packages.Continue '1.0.0' 'any'
        Install-Baseline $packages.Self '1.0.0' 'x86'

        New-PackageArchive $packages.Architecture '2.0.0' 'x86' 'x86-2.0.0' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Architecture '2.0.0' 'x64' 'x64-2.0.0' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Architecture '3.0.0' 'any' 'wrong-any' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Prerelease '1.1.0' 'any' 'stable-1.1.0' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Prerelease '1.2.0-beta.2' 'any' 'beta-2' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Prerelease '1.2.0-beta.10' 'any' 'beta-10' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Selector '1.1.0' 'x86' 'selected-x86' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Selector '1.2.0' 'x86' 'unselected-x86' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Selector '1.2.0' 'x64' 'x64-untouched' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Conflict '2.0.0' 'any' 'conflict-any' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Failure '2.0.0' 'any' 'failed-2.0.0' -ExitCode 23 -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Continue '2.0.0' 'any' 'continued-2.0.0' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Named '1.0.0' 'x86' 'named-x86-1.0.0' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Named '2.0.0' 'x86' 'named-x86-2.0.0' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Named '2.0.0' 'any' 'named-any-2.0.0' -RepositoryEntry | Out-Null
        New-PackageArchive $packages.Self '2.0.0' 'x86' 'self-2.0.0' -IncludeWpmExe -RepositoryEntry | Out-Null
        $entries.Add([ordered]@{ name=$packages.Prerelease; version='not-semver'; arch='any'; url='packages/invalid.zip' })
        'Baseline and candidate archives created.'
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Configure controlled upgrade repository' -Arguments @('repo','add',$repository,'--priority','100') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0) { throw "Repository configuration failed. $Output" }
    }
    $results += New-WpmManualStep -Name 'Seed controlled offline repository index' -Action {
        $path = Get-RepositoryCachePath $dataDir $repository
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
        @{ version=1; packages=@($entries) } | ConvertTo-Json -Depth 5 -Compress | Set-Content -LiteralPath $path -NoNewline
        "Seeded $($entries.Count) repository entries."
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject unsigned upgrade before script execution' -Arguments @('upgrade',$packages.Architecture,'--offline') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -eq 0 -or $Output -notmatch '(?i)unsigned|signature') { throw 'Unsigned upgrade was not rejected.' }
        if ((Get-Content -Raw -LiteralPath (Join-Path $markerDir "$($packages.Architecture)-x86.txt")) -match '2\.0\.0') { throw 'Rejected candidate executed its script.' }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Upgrade all installed specific architectures and ignore any candidate' -Arguments @('upgrade',$packages.Architecture,'--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0) { throw "Architecture upgrade failed. $Output" }
        foreach ($arch in 'x86','x64') {
            $marker = Get-Content -Raw -LiteralPath (Join-Path $markerDir "$($packages.Architecture)-$arch.txt")
            if ($marker -notmatch "$arch-2\.0\.0") { throw "$arch identity was not upgraded." }
        }
        if ($Output -match '3\.0\.0') { throw 'The any candidate participated in a specific-architecture upgrade.' }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject conflicting any installation' -Arguments @('install',$packages.Conflict,'--arch','any','--version','2.0.0','--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -eq 0 -or $Output -notmatch '(?i)conflict|architecture') { throw 'Conflicting any installation was not rejected.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Do not upgrade specific identity from any candidate' -Arguments @('upgrade',$packages.Conflict,'--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0 -or $Output -notmatch '(?i)current') { throw "Expected a successful current result. $Output" }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Install exact named architecture and version' -Arguments @('install',$packages.Named,'--arch','x86','--version','1.0.0','--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0) { throw "Exact named installation failed. $Output" }
        $marker = Get-Content -Raw -LiteralPath (Join-Path $markerDir "$($packages.Named)-x86.txt")
        if ($marker -notmatch 'named-x86-1\.0\.0' -or $marker -match '2\.0\.0') { throw 'Named selectors chose the wrong artifact.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Upgrade one exact architecture to one exact version' -Arguments @('upgrade',$packages.Selector,'--arch','x86','--version','1.1.0','--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0) { throw "Exact upgrade failed. $Output" }
        $x86 = Get-Content -Raw -LiteralPath (Join-Path $markerDir "$($packages.Selector)-x86.txt")
        $x64 = Get-Content -Raw -LiteralPath (Join-Path $markerDir "$($packages.Selector)-x64.txt")
        if ($x86 -notmatch 'selected-x86' -or $x86 -match 'unselected' -or $x64 -match 'untouched') { throw 'Upgrade selectors did not remain exact.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject equal-version upgrade request' -Arguments @('upgrade',$packages.Selector,'--arch','x86','--version','1.1.0','--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -eq 0 -or $Output -notmatch '(?i)newer|current|equal') { throw 'Equal-precedence upgrade was accepted.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject selectors for ZIP path installation' -Arguments @('install',(Join-Path $outputDir "$($packages.Selector)-x86-1.1.0.zip"),'--arch','x86','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -eq 0) { throw 'A selector was accepted for ZIP-path installation.' }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Read default prerelease configuration' -Arguments @('config','get','prerelease') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0 -or $Output -notmatch '(?i)false') { throw 'Prereleases were not disabled by default.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Select stable candidate while prereleases are disabled' -Arguments @('upgrade',$packages.Prerelease,'--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0) { throw "Stable upgrade failed. $Output" }
        $marker = Get-Content -Raw -LiteralPath (Join-Path $markerDir "$($packages.Prerelease)-any.txt")
        if ($marker -notmatch 'stable-1\.1\.0' -or $marker -match 'beta') { throw 'Disabled prerelease filtering chose the wrong candidate.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Enable package prereleases' -Arguments @('config','set','prerelease','true','--package',$packages.Prerelease) -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0) { throw "Could not set package prerelease override. $Output" }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Report effective package prerelease override' -Arguments @('config','get','prerelease','--package',$packages.Prerelease) -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0 -or $Output -notmatch '(?i)true' -or $Output -notmatch '(?i)package|override') { throw 'Effective override and source were not reported.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Use SemVer numeric prerelease ordering' -Arguments @('upgrade',$packages.Prerelease,'--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0) { throw "Prerelease upgrade failed. $Output" }
        $marker = Get-Content -Raw -LiteralPath (Join-Path $markerDir "$($packages.Prerelease)-any.txt")
        if ($marker -notmatch 'beta-10') { throw 'SemVer did not order beta.10 after beta.2.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Remove package prerelease override' -Arguments @('config','unset','prerelease','--package',$packages.Prerelease) -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0) { throw "Could not remove package override. $Output" }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Continue multi-package upgrade after script failure' -Arguments @('upgrade',$packages.Failure,$packages.Continue,'--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -eq 0) { throw 'Partial multi-package failure returned success.' }
        $continued = Get-Content -Raw -LiteralPath (Join-Path $markerDir "$($packages.Continue)-any.txt")
        if ($continued -notmatch 'continued-2\.0\.0') { throw 'Processing stopped after the first package failure.' }
        if ($Output -notmatch '(?i)failed' -or $Output -notmatch '(?i)upgraded') { throw 'Per-package result summary was incomplete.' }
        $storedFailure = Join-Path $dataDir "packages\$($packages.Failure)-any-2.0.0.zip"
        if (Test-Path -LiteralPath $storedFailure) { throw 'Failed candidate was retained as successfully installed.' }
        $failureAudit = Get-ChildItem -LiteralPath (Join-Path $dataDir 'audit') -Filter '*.upgrade-failed.txt' -ErrorAction SilentlyContinue
        if (-not $failureAudit) { throw 'Failed upgrade audit was not created.' }
        if ((Get-Content -Raw -LiteralPath $failureAudit[-1].FullName) -notmatch '(?m)^exit-code=23$') { throw 'Failed audit did not record the script exit code.' }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Complete WPM self-upgrade through cached executable handoff' -Arguments @('upgrade','wpm','--offline','--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if ($ExitCode -ne 0 -or $Output -notmatch '(?i)scheduled|continue') { throw "WPM self-upgrade was not scheduled. $Output" }
        $markerPath = Join-Path $markerDir 'wpm-x86.txt'
        $deadline = [DateTime]::UtcNow.AddSeconds(15)
        while ([DateTime]::UtcNow -lt $deadline) {
            if ((Test-Path -LiteralPath $markerPath) -and (Get-Content -Raw -LiteralPath $markerPath) -match 'self-2\.0\.0') { break }
            Start-Sleep -Milliseconds 100
        }
        if (-not (Test-Path -LiteralPath $markerPath) -or (Get-Content -Raw -LiteralPath $markerPath) -notmatch 'self-2\.0\.0') { throw 'Cached WPM handoff did not complete the installation.' }
        if (-not (Test-Path -LiteralPath (Join-Path $dataDir 'cache\self-upgrade\wpm-x86-2.0.0.exe'))) { throw 'Candidate WPM executable was not retained in the self-upgrade cache.' }
    }

    $results += New-WpmManualStep -Name 'Verify retained versions, upgrade audits, and staging cleanup' -Action {
        foreach ($arch in 'x86','x64') {
            foreach ($version in '1.0.0','2.0.0') {
                if (-not (Test-Path -LiteralPath (Join-Path $dataDir "packages\$($packages.Architecture)-$arch-$version.zip"))) {
                    throw "Missing retained $arch $version archive."
                }
            }
        }
        $auditText = (Get-ChildItem -LiteralPath (Join-Path $dataDir 'audit') -Filter '*.upgrade.txt' | Get-Content -Raw) -join "`n"
        if ($auditText -notmatch '(?m)^old-version=1\.0\.0$' -or $auditText -notmatch '(?m)^new-version=2\.0\.0$') { throw 'Successful upgrade audit did not link versions.' }
        $stagingItems = @(Get-ChildItem -LiteralPath (Join-Path $dataDir 'temp') -Force -ErrorAction SilentlyContinue)
        if ($stagingItems.Count -ne 0) { throw 'Upgrade staging content was not cleaned.' }
        'Retained archives, audit linkage, and staging cleanup verified.'
    }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) { Write-WpmTestEvidence -TestCaseId 'TC-0013' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex }
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
    if ($null -eq $previousDataDir) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue } else { $env:WPM_DATA_DIR = $previousDataDir }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
