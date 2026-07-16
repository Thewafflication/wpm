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
$packageName = "wpm-install-$testId"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-tests-$testId"
$wpmDataDir = Join-Path $testRoot 'wpm-data'
$sourceDir = Join-Path $testRoot $packageName
$outputDir = Join-Path $testRoot 'packages'
$deploymentDir = Join-Path $testRoot 'deployment'
$deploymentFile = Join-Path $deploymentDir 'hello.txt'
$archivePackageName = "$packageName-any-1.2.3"
$archivePath = Join-Path $outputDir "$archivePackageName.zip"
$stagingDir = Join-Path $wpmDataDir "temp\$archivePackageName"
$storedArchivePath = Join-Path $wpmDataDir "packages\$archivePackageName.zip"
$previousWpmDataDir = $env:WPM_DATA_DIR

function Assert-FileContent {
    param(
        [string]$Path,
        [string]$Expected
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Expected file does not exist: $Path"
    }

    $actual = (Get-Content -Raw -LiteralPath $Path).Trim()
    if ($actual -ne $Expected) {
        throw "Unexpected content in $Path. Expected '$Expected', got '$actual'."
    }
}

$started = Get-Date
$results = @()

try {
    $env:WPM_DATA_DIR = $wpmDataDir
    New-Item -ItemType Directory -Force -Path $sourceDir, $outputDir, $deploymentDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir '.wpm') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir 'nested') | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\package.txt') -Value @(
        "name=$packageName"
        'version=1.2.3'
        'arch=any'
        'debug=false'
    )
    Set-Content -LiteralPath (Join-Path $sourceDir 'hello.txt') -Value 'hello from wpm'
    Set-Content -LiteralPath (Join-Path $sourceDir 'nested\data.txt') -Value 'nested package data'
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\install.cmd') -Value @(
        '@echo off'
        "copy /y `"hello.txt`" `"$deploymentFile`" >nul"
    )

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Build setup archive for installation' `
        -Arguments @('build', $sourceDir, $outputDir) `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
                throw "build did not create $archivePath"
            }
        }

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Stage, verify, install, and store archive' `
        -Arguments @('install', $archivePath, '--verbose', '--allow-unsigned') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch '(?i)warning.*unsigned') { throw 'Expected unsigned-package warning.' }
            Assert-FileContent $deploymentFile 'hello from wpm'
            if (-not (Test-Path -LiteralPath $storedArchivePath -PathType Leaf)) {
                throw "install did not store $storedArchivePath"
            }
            if (Test-Path -LiteralPath $stagingDir) {
                throw "install did not remove staging directory $stagingDir"
            }
            if ($Output -notmatch 'Extracting file:' -or $Output -notmatch 'Verifying file:' -or
                $Output -notmatch 'Running install script:' -or $Output -notmatch 'Storing archive:') {
                throw 'Expected verbose installation progress.'
            }
        }

}
finally {
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0004' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
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
