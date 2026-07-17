param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,

    [string]$EvidenceTex,

    [switch]$NoFailOnFailure
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$started = Get-Date
$results = @(
    Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Invoke wpm with no command-line arguments' `
        -Arguments @() `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch 'Waughtal Package Manager .* Version ' -or $Output -notmatch 'Usage:') {
                throw 'Expected version and usage information in output.'
            }
        }

    Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Invoke wpm --version' `
        -Arguments @('--version') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch 'Dependencies:' -or
                $Output -notmatch 'miniz .+commit ' -or
                $Output -notmatch 'libsodium .+commit ' -or
                $Output -notmatch 'urlmon \d+\.\d+\.\d+\.\d+ \(Windows system library\)' -or
                $Output -notmatch 'advapi32 \d+\.\d+\.\d+\.\d+ \(Windows system library\)') {
                throw 'Expected dependency version information in output.'
            }
        }

    Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Invoke wpm --verbose' `
        -Arguments @('--verbose') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch 'Runtime mode: portable') {
                throw 'Expected portable runtime mode information for the build output.'
            }
        }

    Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Invoke the CI version and runtime diagnostic combination' `
        -Arguments @('--version', '--verbose') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch 'Waughtal Package Manager .* Version ' -or
                $Output -notmatch 'Dependencies:' -or
                $Output -notmatch 'Runtime mode: portable') {
                throw 'Expected combined version, dependency, and runtime information.'
            }
        }
)
$finished = Get-Date

if ($EvidenceTex) {
    Write-WpmTestEvidence -TestCaseId 'TC-0001' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
