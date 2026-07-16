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
$packageName = "wpm-remove-$testId"
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

$started = Get-Date
$results = @()

try {
    $env:WPM_DATA_DIR = $wpmDataDir
    New-Item -ItemType Directory -Force -Path $sourceDir, $outputDir, $deploymentDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\package.txt') -Value @(
        "name=$packageName"
        'version=1.2.3'
        'arch=any'
        'debug=false'
    )
    Set-Content -LiteralPath (Join-Path $sourceDir 'hello.txt') -Value 'hello from wpm'
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\install.cmd') -Value @(
        '@echo off'
        "copy /y `"hello.txt`" `"$deploymentFile`" >nul"
    )
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\remove.cmd') -Value @(
        '@echo off'
        "del /q `"$deploymentFile`""
    )

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Build package for removal' `
        -Arguments @('build', $sourceDir, $outputDir, '--no-index') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0 -or -not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
                throw 'Failed to build the package used for removal.'
            }
        }

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Install package before removal' `
        -Arguments @('install', $archivePath, '--allow-unsigned') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) { throw "Expected install exit code 0, got $ExitCode." }
            if (-not (Test-Path -LiteralPath $deploymentFile -PathType Leaf)) {
                throw 'Install script did not create the deployment file.'
            }
            if (-not (Test-Path -LiteralPath $storedArchivePath -PathType Leaf)) {
                throw 'Install did not retain the package archive.'
            }
        }

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Remove retained package by archive name' `
        -Arguments @('remove', "$archivePackageName.zip") `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) { throw "Expected removal exit code 0, got $ExitCode." }
            if (Test-Path -LiteralPath $deploymentFile) {
                throw 'Remove script did not remove the deployment file.'
            }
            if (Test-Path -LiteralPath $storedArchivePath) {
                throw 'Removal did not delete the retained archive.'
            }
            if (Test-Path -LiteralPath $stagingDir) {
                throw 'Removal did not clean its staging directory.'
            }
        }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0008' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
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
