param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,

    [string]$EvidenceTex,

    [switch]$NoFailOnFailure
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$testId = [Guid]::NewGuid().ToString('N')
$packageName = "local-repository-$testId"
$traversalName = "local-traversal-$testId"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-local-repository-$testId"
$dataDir = Join-Path $testRoot 'wpm-data'
$sourceDir = Join-Path $testRoot 'source'
$repositoryDir = Join-Path $testRoot 'repository with spaces'
$deployment = Join-Path $testRoot 'deployment.txt'
$relativeRepository = '.\repository with spaces'
$previousDataDir = $env:WPM_DATA_DIR
$started = Get-Date
$results = @()
$archive = $null
$index = $null

try {
    $env:WPM_DATA_DIR = $dataDir
    New-Item -ItemType Directory -Force -Path $sourceDir, $repositoryDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\package.txt') -Value @(
        "name=$packageName"
        'version=1.0.0'
        'arch=any'
        'debug=false'
    )
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\install.cmd') -Value @(
        '@echo off'
        "echo installed-from-local-repository> `"$deployment`""
    )

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build local repository package' -Arguments @('build', $sourceDir, $repositoryDir) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Local repository build failed. $Output" }
    }

    $archive = Get-ChildItem -LiteralPath $repositoryDir -Filter '*.zip' | Select-Object -First 1
    if (-not $archive) { throw 'Local repository package archive was not created.' }
    $index = Join-Path $repositoryDir 'index.json'
    Set-Content -NoNewline -LiteralPath $index -Value (
        '{"version":1,"packages":[{"name":"' + $packageName +
        '","version":"1.0.0","arch":"any","url":"' + $archive.Name +
        '"},{"name":"' + $traversalName +
        '","version":"1.0.0","arch":"any","url":"../' + $archive.Name + '"}]}'
    )
    $indexHash = (Get-FileHash -LiteralPath $index -Algorithm SHA256).Hash
    $archiveHash = (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA256).Hash
    (Get-Item -LiteralPath $index).Attributes =
        (Get-Item -LiteralPath $index).Attributes -bor [IO.FileAttributes]::ReadOnly
    $archive.Attributes = $archive.Attributes -bor [IO.FileAttributes]::ReadOnly

    Push-Location $testRoot
    try {
        $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Add relative local repository' -Arguments @('repo', 'add', $relativeRepository) -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0 -or $Output -notmatch [regex]::Escape($repositoryDir)) {
                throw "Relative repository was not resolved to its stable absolute path. $Output"
            }
        }
    }
    finally {
        Pop-Location
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reprioritize using absolute local path' -Arguments @('repo', 'add', $repositoryDir, '--priority', '7') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch 'Repository updated') {
            throw "Absolute repository path did not identify the relative-path entry. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'List canonical local repository' -Arguments @('repo', 'list') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch "7\s+$([regex]::Escape($repositoryDir))") {
            throw "Canonical local repository and priority were not listed. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Refresh read-only local repository while offline' -Arguments @('repo', 'update', '--offline') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch [regex]::Escape($repositoryDir)) {
            throw "Read-only local repository refresh failed. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Retain signature policy for local repository' -Arguments @('install', $packageName, '--offline') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0) { throw 'Unsigned local package was trusted implicitly.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Install named package from local repository' -Arguments @('install', $packageName, '--offline', '--allow-unsigned') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Local repository installation failed. $Output" }
        if ((Get-Content -Raw -LiteralPath $deployment).Trim() -ne 'installed-from-local-repository') {
            throw 'Local package install script did not run.'
        }
        if ($Output -notmatch [regex]::Escape($repositoryDir)) {
            throw 'Installation output did not identify the local source.'
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject package path escaping local repository' -Arguments @('install', $traversalName, '--offline', '--allow-unsigned') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'invalid package URL') {
            throw "Escaping local package path was not rejected. $Output"
        }
    }

    $results += New-WpmManualStep -Name 'Verify read-only source was not modified' -Action {
        if ((Get-FileHash -LiteralPath $index -Algorithm SHA256).Hash -ne $indexHash -or
            (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA256).Hash -ne $archiveHash) {
            throw 'Local repository source content changed during refresh or installation.'
        }
        'Read-only repository hashes preserved.'
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject unsupported file URL' -Arguments @('repo', 'add', "file:///$($repositoryDir.Replace('\', '/'))") -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'must use https://, opted-in http://, or a filesystem path') {
            throw "Unsupported file URL was not rejected clearly. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject device path' -Arguments @('repo', 'add', '\\.\C:\') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'device namespace') {
            throw "Device path was not rejected. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Remove local repository by absolute path' -Arguments @('repo', 'remove', $repositoryDir) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch 'Repository removed') {
            throw "Local repository removal failed. $Output"
        }
    }
}
finally {
    $finished = Get-Date
    if ($archive) { $archive.Attributes = $archive.Attributes -band (-bnot [IO.FileAttributes]::ReadOnly) }
    if ($index -and (Test-Path -LiteralPath $index)) {
        $indexItem = Get-Item -LiteralPath $index
        $indexItem.Attributes = $indexItem.Attributes -band (-bnot [IO.FileAttributes]::ReadOnly)
    }
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0024' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
    if ($null -eq $previousDataDir) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue }
    else { $env:WPM_DATA_DIR = $previousDataDir }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
