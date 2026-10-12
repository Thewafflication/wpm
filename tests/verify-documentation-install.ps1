<#
.SYNOPSIS
Verify documentation bootstrap retries missing executables after exit zero.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$workflow = Get-Content -LiteralPath (Join-Path $workspace `
    '.github/workflows/documentation.yml') -Raw
$start = $workflow.IndexOf('          $documentationSessionPath = $env:Path')
$end = $workflow.IndexOf('          Install-ChocolateyPackage pandoc', $start)
if ($start -lt 0 -or $end -le $start) {
    throw 'Documentation bootstrap function could not be located.'
}
$bootstrap = [scriptblock]::Create($workflow.Substring($start, $end - $start))
$fixture = Join-Path $workspace ('out/documentation-install-' + `
    [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$toolName = 'wpm-pdf-fixture-' + [Guid]::NewGuid().ToString('N') + '.cmd'
$fixtureTool = Join-Path $fixture $toolName
$savedPath = $env:Path
$env:Path = "$fixture;$savedPath"

function choco {
    $script:fixtureAttempts++
    $global:LASTEXITCODE = $script:fixtureExitCode
    if ($script:fixtureCreateTool -and $script:fixtureAttempts -eq 2) {
        Set-Content -LiteralPath $fixtureTool -Encoding ascii -Value `
            '@echo off', 'exit /b 0'
    }
}

function Start-Sleep {
    param([int]$Seconds)
    if ($Seconds -ne 15) { throw 'Unexpected retry interval.' }
}

try {
    . $bootstrap
    $script:fixtureAttempts = 0
    $script:fixtureExitCode = 0
    $script:fixtureCreateTool = $true
    Install-ChocolateyPackage fixture -Commands $toolName
    if ($script:fixtureAttempts -ne 2 -or
        -not $env:Path.Contains($fixture)) {
        throw 'Missing-tool retry or session PATH preservation failed.'
    }

    foreach ($scenario in @(
        @{ ExitCode = 7; Command = $toolName },
        @{ ExitCode = 0; Command = 'missing-' + $toolName }
    )) {
        $script:fixtureAttempts = 0
        $script:fixtureExitCode = $scenario.ExitCode
        $script:fixtureCreateTool = $false
        $failed = $false
        try {
            Install-ChocolateyPackage fixture -Commands $scenario.Command
        } catch {
            $failed = $_.Exception.Message.Contains('after 3 attempts')
        }
        if (-not $failed -or $script:fixtureAttempts -ne 3) {
            throw 'Incomplete documentation installation did not fail closed.'
        }
    }
} finally {
    $env:Path = $savedPath
}
Write-Host 'PASS: executable admission, bounded retry, and session PATH'
