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
$packageName = "wpm-index-$testId"
$tamperedPackageName = "$packageName-tampered"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-tests-$testId"
$wpmDataDir = Join-Path $testRoot 'wpm-data'
$sourceDir = Join-Path $testRoot $packageName
$outputDir = Join-Path $testRoot 'packages'
$inspectDir = Join-Path $testRoot 'inspect'
$tamperDir = Join-Path $testRoot 'tamper'
$archivePackageName = "$packageName-1.2.3-any"
$archivePath = Join-Path $outputDir "$archivePackageName.zip"
$tamperedArchivePath = Join-Path $outputDir "$tamperedPackageName.zip"
$stagingDir = Join-Path $wpmDataDir "temp\$archivePackageName"
$tamperedStagingDir = Join-Path $wpmDataDir "temp\$tamperedPackageName"
$storedArchivePath = Join-Path $wpmDataDir "packages\$archivePackageName.zip"
$previousWpmDataDir = $env:WPM_DATA_DIR

$started = Get-Date
$results = @()

try {
    $env:WPM_DATA_DIR = $wpmDataDir
    New-Item -ItemType Directory -Force -Path $sourceDir, $outputDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir '.wpm') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir 'nested') | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceDir 'hello.txt') -Value 'hello from wpm'
    Set-Content -LiteralPath (Join-Path $sourceDir 'nested\data.txt') -Value 'nested package data'
    Set-Content -LiteralPath (Join-Path $sourceDir 'ignored.txt') -Value 'ignored package data'
    Set-Content -LiteralPath (Join-Path $sourceDir 'trace.log') -Value 'ignored log data'
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\package.txt') -Value @(
        "name=$packageName"
        'version=1.2.3'
        'arch=any'
        'debug=false'
    )
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\install.cmd') -Value '@echo off'
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\remove.cmd') -Value '@echo off'
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\wpmignore.txt') -Value @(
        '.wpm/'
        'ignored.txt'
        '*.log'
    )

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Build archive and populate BLAKE2b index' `
        -Arguments @('build', $sourceDir, $outputDir) `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
                throw "build did not create $archivePath"
            }

            $indexPath = Join-Path $sourceDir '.wpm\index.csv'
            if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
                throw 'build did not populate .wpm\index.csv'
            }

            $index = Get-Content -LiteralPath $indexPath
            if ($index[0] -ne 'filename,size,hash,algorithm') {
                throw "Unexpected index header: $($index[0])"
            }
            if (($index -join "`n") -notmatch '(?m)^hello\.txt,\d+,[0-9a-f]{64},blake2b$') {
                throw 'index does not contain a BLAKE2b signature for hello.txt'
            }
            if (($index -join "`n") -notmatch '(?m)^nested/data\.txt,\d+,[0-9a-f]{64},blake2b$') {
                throw 'index does not contain a BLAKE2b signature for nested/data.txt'
            }
            foreach ($supportFile in @(
                '\.wpm/package\.txt',
                '\.wpm/install\.cmd',
                '\.wpm/remove\.cmd',
                '\.wpm/wpmignore\.txt'
            )) {
                if (($index -join "`n") -notmatch "(?m)^$supportFile,\d+,[0-9a-f]{64},blake2b$") {
                    throw "index does not contain a relative-path signature for $supportFile"
                }
            }
            if (($index -join "`n") -match '(?m)^\.wpm/index\.csv,') {
                throw 'index.csv should not index itself'
            }
            if (($index -join "`n") -match '(?m)^(ignored\.txt|trace\.log),') {
                throw '.wpmignore entries were unexpectedly indexed'
            }
        }

    $results += New-WpmManualStep `
        -Name 'Inspect archive index contents' `
        -Action {
            Expand-Archive -LiteralPath $archivePath -DestinationPath $inspectDir -Force
            $archiveIndexPath = Join-Path $inspectDir '.wpm\index.csv'
            if (-not (Test-Path -LiteralPath $archiveIndexPath -PathType Leaf)) {
                throw 'archive does not contain .wpm\index.csv'
            }
            $archiveIndex = Get-Content -Raw -LiteralPath $archiveIndexPath
            if ($archiveIndex -notmatch '(?m)^hello\.txt,\d+,[0-9a-f]{64},blake2b\r?$') {
                throw 'archive index does not contain hello.txt signature'
            }
            'Archive index contains BLAKE2b signatures.'
        }

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Install archive with valid signatures' `
        -Arguments @('install', $archivePath) `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if (-not (Test-Path -LiteralPath $storedArchivePath -PathType Leaf)) {
                throw 'valid install did not retain the archive'
            }
            if (Test-Path -LiteralPath $stagingDir) {
                throw 'valid install did not remove its staging directory'
            }
        }

    $results += New-WpmManualStep `
        -Name 'Create tampered archive' `
        -Action {
            Expand-Archive -LiteralPath $archivePath -DestinationPath $tamperDir -Force
            Set-Content -LiteralPath (Join-Path $tamperDir 'hello.txt') -Value 'tampered content'
            Compress-Archive -Path (Join-Path $tamperDir '*') -DestinationPath $tamperedArchivePath -Force
            if (-not (Test-Path -LiteralPath $tamperedArchivePath -PathType Leaf)) {
                throw 'tampered archive was not created'
            }
            "Tampered archive created: $tamperedArchivePath"
        }

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Reject archive with tampered indexed file' `
        -Arguments @('install', $tamperedArchivePath) `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -eq 0) {
                throw 'Expected tampered install to fail.'
            }
            if ($Output -notmatch 'signature verification failed') {
                throw 'Tampered install did not report signature verification failure.'
            }
            if (Test-Path -LiteralPath $tamperedStagingDir) {
                throw 'tampered install did not remove its staging directory'
            }
        }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0005' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
    if ($null -eq $previousWpmDataDir) {
        Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue
    }
    else {
        $env:WPM_DATA_DIR = $previousWpmDataDir
    }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
