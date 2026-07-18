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
$packageName = "wpm-name-$testId"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-tests-$testId"
$sourceDir = Join-Path $testRoot 'source'
$outputDir = Join-Path $testRoot 'packages'
$releaseArchivePath = Join-Path $outputDir "$packageName-x64-2.4.6+build.5.zip"
$debugArchivePath = Join-Path $outputDir "$packageName-x64-debug-2.4.6+build.5.zip"

function Set-PackageMetadata {
    param([bool]$Debug)

    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\package.txt') -Value @(
        "name=$packageName"
        'version=2.4.6+build.5'
        'arch=x64'
        "debug=$($Debug.ToString().ToLowerInvariant())"
    )
}

$started = Get-Date
$results = @()

try {
    New-Item -ItemType Directory -Force -Path $sourceDir, $outputDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceDir 'hello.txt') -Value 'hello from wpm'
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\wpmignore.txt') -Value '*.log'
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\install.cmd') -Value '@echo off'
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\remove.cmd') -Value '@echo off'

    Set-PackageMetadata -Debug:$false
    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Build release archive using package metadata name' `
        -Arguments @('build', $sourceDir, $outputDir, '--no-index') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if (-not (Test-Path -LiteralPath $releaseArchivePath -PathType Leaf)) {
                throw "build did not create $releaseArchivePath"
            }
        }

    Set-PackageMetadata -Debug:$true
    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Build debug archive using package metadata name' `
        -Arguments @('build', $sourceDir, $outputDir, '--no-index') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if (-not (Test-Path -LiteralPath $debugArchivePath -PathType Leaf)) {
                throw "build did not create $debugArchivePath"
            }
        }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0006' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
