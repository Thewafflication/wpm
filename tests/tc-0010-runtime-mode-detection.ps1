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
$dataDir = Join-Path ([IO.Path]::GetTempPath()) "wpm-runtime-data-$testId"
$previousDataDir = $env:WPM_DATA_DIR
$started = Get-Date
$results = @()

try {
    $env:WPM_DATA_DIR = $dataDir
    $results += @(
    Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Report portable runtime mode with verbose output' `
        -Arguments @('--verbose') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch 'Runtime mode: portable' -or $Output -notmatch 'Executable: ') {
                throw 'Expected verbose portable runtime mode and executable path information.'
            }
        }
    )

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Initialize portable runtime data directories without network access' `
        -Arguments @('repo', 'list') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            foreach ($directory in @($dataDir, (Join-Path $dataDir 'packages'), (Join-Path $dataDir 'temp'), (Join-Path $dataDir 'cache'), (Join-Path $dataDir 'config'))) {
                if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
                    throw "Portable runtime did not initialize $directory"
                }
            }
        }
}
finally {
    $env:WPM_DATA_DIR = $previousDataDir
    if (Test-Path -LiteralPath $dataDir) {
        Remove-Item -LiteralPath $dataDir -Recurse -Force
    }
}
$finished = Get-Date

if ($EvidenceTex) {
    Write-WpmTestEvidence -TestCaseId 'TC-0010' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
