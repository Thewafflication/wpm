param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,
    [string]$EvidenceTex,
    [switch]$NoFailOnFailure
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("wpm-task-help-" + [Guid]::NewGuid().ToString('N'))
$previousDataDir = $env:WPM_DATA_DIR
$env:WPM_DATA_DIR = Join-Path $testRoot 'must-not-exist'
$started = Get-Date
$results = @()

try {
    $commands = @('init', 'build', 'verify', 'install', 'remove', 'repo', 'keygen', 'key', 'trust', 'config', 'update', 'upgrade')
    foreach ($command in $commands) {
        $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name "Show task help for $command" -Arguments @('help', $command) -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0 -or $Output -notmatch "Usage: wpm $command" -or
                $Output -notmatch 'Example(?:s)?:') {
                throw "Task help for $command is incomplete. $Output"
            }
        }
        $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name "Show inline help for $command" -Arguments @($command, '--help') -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0 -or $Output -notmatch "Usage: wpm $command" -or
                $Output -notmatch 'Example(?:s)?:') {
                throw "Inline help for $command is incomplete. $Output"
            }
        }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Show complete common-task examples' -Arguments @('--help') -Assert {
        param($ExitCode, $Output)
        $required = @(
            'wpm init ', 'wpm build ', 'wpm verify ', 'wpm install ', 'wpm remove ',
            'wpm repo add ', 'wpm repo update', 'wpm keygen ', 'wpm key default ',
            'wpm trust add ', 'wpm config set ', 'wpm update', 'wpm upgrade ',
            'wpm --diagnose'
        )
        if ($ExitCode -ne 0) { throw "Full help failed. $Output" }
        foreach ($example in $required) {
            if (-not $Output.Contains($example)) { throw "Missing task example '$example'." }
        }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject unknown help topic narrowly' -Arguments @('help', 'not-a-command') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'Error: unknown help topic: not-a-command' -or
            $Output -notmatch 'Usage: wpm help \[command\]' -or $Output -match 'Commands:') {
            throw "Unknown help topic diagnostic is not narrow. $Output"
        }
    }

    $results += New-WpmManualStep -Name 'Help paths do not initialize durable state' -Action {
        if (Test-Path -LiteralPath $env:WPM_DATA_DIR) {
            throw 'A help-only invocation initialized the WPM data directory.'
        }
        'No WPM data directory was created.'
    }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0027' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
    if ($null -eq $previousDataDir) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue }
    else { $env:WPM_DATA_DIR = $previousDataDir }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
