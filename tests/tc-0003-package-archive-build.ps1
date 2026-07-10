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
$packageName = "wpm-build-$testId"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-tests-$testId"
$sourceDir = Join-Path $testRoot $packageName
$outputDir = Join-Path $testRoot 'packages'
$extractDir = Join-Path $testRoot 'inspect'
$archivePath = Join-Path $outputDir "$packageName.zip"

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
    New-Item -ItemType Directory -Force -Path $sourceDir, $outputDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir 'nested') | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceDir 'hello.txt') -Value 'hello from wpm'
    Set-Content -LiteralPath (Join-Path $sourceDir 'nested\data.txt') -Value 'nested package data'

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Build archive from source directory' `
        -Arguments @('build', $sourceDir, $outputDir, '--no-index') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
                throw "build did not create $archivePath"
            }
        }

    $results += New-WpmManualStep `
        -Name 'Inspect generated archive contents' `
        -Action {
            Expand-Archive -LiteralPath $archivePath -DestinationPath $extractDir -Force
            Assert-FileContent (Join-Path $extractDir 'hello.txt') 'hello from wpm'
            Assert-FileContent (Join-Path $extractDir 'nested\data.txt') 'nested package data'
            "Archive contents verified: $archivePath"
        }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0003' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
