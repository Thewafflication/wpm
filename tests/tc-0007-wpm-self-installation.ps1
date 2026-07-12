param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,

    [string]$EvidenceTex,

    [switch]$NoFailOnFailure
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$setupCmd = Join-Path $repositoryRoot 'setup.cmd'
$removeCmd = Join-Path $repositoryRoot 'remove.cmd'
$testId = [Guid]::NewGuid().ToString('N')
$installDir = Join-Path ([IO.Path]::GetTempPath()) "wpm-self-install-$testId"
$dataDir = Join-Path ([IO.Path]::GetTempPath()) "wpm-self-data-$testId"

function Invoke-CmdScript {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Script,

        [string[]]$Arguments = @()
    )

    & $Script @Arguments 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "$(Split-Path -Leaf $Script) exited $LASTEXITCODE."
    }
}

$started = Get-Date
$results = @()
$previousInstallDir = $env:WPM_INSTALL_DIR
$previousDataDir = $env:WPM_DATA_DIR

try {
    $env:WPM_INSTALL_DIR = $installDir
    $env:WPM_DATA_DIR = $dataDir

    $results += New-WpmManualStep -Name 'Install WPM with setup.cmd' -Action {
        Invoke-CmdScript -Script $setupCmd -Arguments @($WpmExe)
        if (-not (Test-Path -LiteralPath (Join-Path $installDir 'wpm.exe') -PathType Leaf)) {
            throw "setup.cmd did not install wpm.exe to $installDir"
        }
        if (-not (Test-Path -LiteralPath $dataDir -PathType Container)) {
            throw "setup.cmd did not create the WPM data directory at $dataDir"
        }
    }

    $installedExe = Join-Path $installDir 'wpm.exe'
    $results += Invoke-WpmTestStep `
        -WpmExe $installedExe `
        -Name 'Invoke the self-installed WPM executable' `
        -Arguments @('--version') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch 'Waughtal Package Manager .* Version ') {
                throw 'Expected installed executable version information.'
            }
        }

    $results += New-WpmManualStep -Name 'Remove WPM with remove.cmd' -Action {
        Invoke-CmdScript -Script $removeCmd
        if (Test-Path -LiteralPath $installDir) {
            throw "remove.cmd did not remove $installDir"
        }
        if (Test-Path -LiteralPath $dataDir) {
            throw "remove.cmd did not remove $dataDir"
        }
    }
}
finally {
    $env:WPM_INSTALL_DIR = $previousInstallDir
    $env:WPM_DATA_DIR = $previousDataDir
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0007' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $installDir) {
        Remove-Item -LiteralPath $installDir -Recurse -Force
    }
    if (Test-Path -LiteralPath $dataDir) {
        Remove-Item -LiteralPath $dataDir -Recurse -Force
    }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
