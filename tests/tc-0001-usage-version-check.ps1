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
$expectsWcrt = Test-Path -LiteralPath (Join-Path (Split-Path -Parent $WpmExe) 'wcrt.dll')
$wcrtVersionPattern = 'wcrt \d+\.\d+\.\d+(?:\.\d+)? \(runtime library\)'
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
            if ($Output -notmatch 'Waughtal Package Manager .* Version ' -or
                $Output -notmatch "Run 'wpm --help'" -or
                $Output -match 'Commands:') {
                throw 'Expected compact version and help-hint output.'
            }
        }

    Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Invoke wpm --help' `
        -Arguments @('--help') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0 -or $Output -notmatch 'Commands:' -or
                $Output -notmatch '--help' -or $Output -notmatch 'Examples:') {
                throw 'Expected complete help information.'
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
                $Output -notmatch 'mbedTLS \d+\.\d+\.\d+ \(bundled TLS 1\.2, embedded CA roots\)' -or
                $Output -notmatch 'minizip-ng .+commit ' -or
                $Output -notmatch 'zlib-ng .+commit ' -or
                $Output -notmatch 'libsodium .+commit ' -or
                $Output -notmatch 'urlmon \d+\.\d+\.\d+\.\d+ \(Windows system library\)' -or
                $Output -notmatch 'advapi32 \d+\.\d+\.\d+\.\d+ \(Windows system library\)') {
                throw 'Expected dependency version information in output.'
            }
            if ($expectsWcrt -and $Output -notmatch $wcrtVersionPattern) {
                throw 'Expected the WCRT runtime dependency version in output.'
            }
            if ($Output -match '(?m)^CPU:') {
                throw 'CPU details should require --version --verbose.'
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
            if ($expectsWcrt -and $Output -notmatch $wcrtVersionPattern) {
                throw 'Expected the WCRT runtime dependency version in combined verbose version output.'
            }
            if ($expectsWcrt -and ($Output -notmatch '(?m)^CPU:' -or
                $Output -notmatch 'Process architecture: (x86|x64|arm64|unknown)' -or
                $Output -notmatch 'Vendor: .+' -or $Output -notmatch 'Brand: .+' -or
                $Output -notmatch 'Logical processors \(system\): ([1-9][0-9]*|unknown)' -or
                $Output -notmatch 'Physical cores \(system\): ([1-9][0-9]*|unknown)' -or
                $Output -notmatch 'Available processors \(process affinity\): ([1-9][0-9]*|unknown)' -or
                $Output -notmatch 'Usable instruction sets: .+')) {
                throw 'Expected CPU identity, processor counts, and usable instruction sets.'
            }
        }
)
$finished = Get-Date

if ($EvidenceTex) {
    Write-WpmTestEvidence -TestCaseId 'TC-0001' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
