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
$portableDir = Join-Path ([IO.Path]::GetTempPath()) "wpm-portable-$testId"
$managedProgramFiles = Join-Path ([IO.Path]::GetTempPath()) "wpm-managed-program-files-$testId"
$portableExe = Join-Path $portableDir 'wpm.exe'
$managedExe = Join-Path $managedProgramFiles 'WPM\wpm.exe'
$previousDataDir = $env:WPM_DATA_DIR
$previousProgramW6432 = $env:ProgramW6432
$portableAcl = $null
$started = Get-Date
$results = @()

try {
    $env:WPM_DATA_DIR = $dataDir
    New-Item -ItemType Directory -Force -Path $portableDir, (Split-Path -Parent $managedExe) | Out-Null
    Copy-Item -LiteralPath $WpmExe -Destination $portableExe
    Copy-Item -LiteralPath $WpmExe -Destination $managedExe

    $results += Invoke-WpmTestStep `
        -WpmExe $portableExe `
        -Name 'Report portable runtime mode with verbose output' `
        -Arguments @('--verbose') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) { throw "Expected exit code 0, got $ExitCode." }
            if ($Output -notmatch 'Runtime mode: portable' -or $Output -notmatch 'Executable: ') {
                throw 'Expected verbose portable runtime mode and executable path information.'
            }
        }

    $env:ProgramW6432 = $managedProgramFiles
    $results += Invoke-WpmTestStep `
        -WpmExe $managedExe `
        -Name 'Report managed runtime mode from the Program Files WPM directory' `
        -Arguments @('--verbose') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) { throw "Expected exit code 0, got $ExitCode." }
            if ($Output -notmatch 'Runtime mode: managed') { throw 'Expected managed runtime mode.' }
        }

    $portableAcl = Get-Acl -LiteralPath $portableDir
    $readOnlyAcl = Get-Acl -LiteralPath $portableDir
    $readOnlyAcl.AddAccessRule([System.Security.AccessControl.FileSystemAccessRule]::new(
        [System.Security.Principal.WindowsIdentity]::GetCurrent().User,
        [System.Security.AccessControl.FileSystemRights]::Write,
        [System.Security.AccessControl.AccessControlType]::Deny
    ))
    Set-Acl -LiteralPath $portableDir -AclObject $readOnlyAcl
    $env:ProgramW6432 = $previousProgramW6432

    $results += Invoke-WpmTestStep `
        -WpmExe $portableExe `
        -Name 'Initialize data from a read-only portable executable directory' `
        -Arguments @('repo', 'list') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) { throw "Expected exit code 0, got $ExitCode." }
            if ((Get-ChildItem -Force -LiteralPath $portableDir).Count -ne 1) {
                throw 'WPM wrote mutable state beside the read-only portable executable.'
            }
            foreach ($directory in @($dataDir, (Join-Path $dataDir 'packages'), (Join-Path $dataDir 'temp'), (Join-Path $dataDir 'cache'), (Join-Path $dataDir 'config'))) {
                if (-not (Test-Path -LiteralPath $directory -PathType Container)) { throw "Read-only portable execution did not initialize $directory" }
            }
        }
}
finally {
    if ($portableAcl -and (Test-Path -LiteralPath $portableDir)) { Set-Acl -LiteralPath $portableDir -AclObject $portableAcl }
    if ($null -eq $previousDataDir) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue } else { $env:WPM_DATA_DIR = $previousDataDir }
    if ($null -eq $previousProgramW6432) { Remove-Item Env:ProgramW6432 -ErrorAction SilentlyContinue } else { $env:ProgramW6432 = $previousProgramW6432 }
    foreach ($path in @($dataDir, $portableDir, $managedProgramFiles)) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
    }
}
$finished = Get-Date

if ($EvidenceTex) { Write-WpmTestEvidence -TestCaseId 'TC-0010' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex }
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
