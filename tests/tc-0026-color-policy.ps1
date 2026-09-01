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
$escape = [string][char]27
$results = @(
    Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Auto color is plain when redirected' -Arguments @('--color', 'auto', 'not-a-command') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output.Contains($escape) -or
            $Output -notmatch 'Error: unknown command: not-a-command' -or
            $Output -notmatch 'Usage: wpm <command>') {
            throw "Auto redirected output contract failed. $Output"
        }
    }

    Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Always color emits styling without losing labels' -Arguments @('not-a-command', '--color=always') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or -not $Output.Contains($escape) -or
            $Output -notmatch 'Error: unknown command: not-a-command') {
            throw "Always color output contract failed. $Output"
        }
    }

    Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Never color suppresses styling' -Arguments @('--color', 'never', 'not-a-command') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output.Contains($escape) -or
            $Output -notmatch 'Error: unknown command: not-a-command') {
            throw "Never color output contract failed. $Output"
        }
    }

    Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Last color option takes precedence' -Arguments @('--color', 'always', 'not-a-command', '--color', 'never') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output.Contains($escape)) {
            throw "Color precedence contract failed. $Output"
        }
    }

    Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject invalid color narrowly' -Arguments @('--color', 'purple', '--help') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'Error: invalid --color value: purple' -or
            $Output -notmatch 'expected auto, always, or never' -or $Output -match 'Commands:') {
            throw "Invalid color diagnostic contract failed. $Output"
        }
    }

    Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Document color policy in help' -Arguments @('--help') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch '--color <auto\|always\|never>') {
            throw "Color help contract failed. $Output"
        }
    }
)
$finished = Get-Date

if ($EvidenceTex) {
    Write-WpmTestEvidence -TestCaseId 'TC-0026' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
