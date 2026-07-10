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
$sourceDir = Join-Path $testRoot $packageName
$outputDir = Join-Path $testRoot 'packages'
$archivePackageName = "$packageName-1.2.3-any"
$archivePath = Join-Path $outputDir "$archivePackageName.zip"
$installDir = Join-Path 'C:\TEMP' $archivePackageName

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

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Build setup archive for installation' `
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

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Install archive to installation root' `
        -Arguments @('install', $archivePath) `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            Assert-FileContent (Join-Path $installDir 'hello.txt') 'hello from wpm'
            Assert-FileContent (Join-Path $installDir 'nested\data.txt') 'nested package data'
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
    if (Test-Path -LiteralPath $installDir) {
        Remove-Item -LiteralPath $installDir -Recurse -Force
    }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
