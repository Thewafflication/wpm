param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,
    [string]$EvidenceTex,
    [switch]$NoFailOnFailure
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("wpm-invalid-options-" + [Guid]::NewGuid().ToString('N'))
$previousDataDir = $env:WPM_DATA_DIR
$env:WPM_DATA_DIR = Join-Path $testRoot 'must-not-exist'
$started = Get-Date
$results = @()

try {
    $commands = @('init', 'build', 'verify', 'install', 'remove', 'repo', 'keygen', 'key', 'trust', 'config', 'update', 'upgrade')
    foreach ($command in $commands) {
        $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name "Reject unknown $command option" -Arguments @($command, '--not-an-option') -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -eq 0 -or
                $Output -notmatch "Error: invalid option for ${command}: --not-an-option" -or
                $Output -notmatch "Usage: wpm $command" -or
                $Output -notmatch "Run 'wpm help $command'" -or
                $Output -match 'Commands:') {
                throw "Unknown $command option did not fail narrowly. $Output"
            }
        }
    }

    $missingValues = @(
        @{ Command = 'build'; Arguments = @('build', 'source', '--sign'); Option = '--sign' },
        @{ Command = 'install'; Arguments = @('install', 'package', '--arch'); Option = '--arch' },
        @{ Command = 'repo'; Arguments = @('repo', 'add', '.\repository', '--priority'); Option = '--priority' },
        @{ Command = 'config'; Arguments = @('config', 'get', 'prerelease', '--package'); Option = '--package' },
        @{ Command = 'upgrade'; Arguments = @('upgrade', 'package', '--version'); Option = '--version' }
    )
    foreach ($case in $missingValues) {
        $command = $case.Command
        $option = $case.Option
        $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name "Reject missing $option value" -Arguments $case.Arguments -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -eq 0 -or
                $Output -notmatch "Error: option $([regex]::Escape($option)) requires a value for $command" -or
                $Output -notmatch "Usage: wpm $command" -or
                $Output -notmatch "Run 'wpm help $command'") {
                throw "Missing $option value did not fail narrowly. $Output"
            }
        }
    }

    $results += New-WpmManualStep -Name 'Option validation does not initialize durable state' -Action {
        if (Test-Path -LiteralPath $env:WPM_DATA_DIR) {
            throw 'Invalid-option handling initialized the WPM data directory.'
        }
        'No WPM data directory was created.'
    }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0028' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
    if ($null -eq $previousDataDir) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue }
    else { $env:WPM_DATA_DIR = $previousDataDir }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
