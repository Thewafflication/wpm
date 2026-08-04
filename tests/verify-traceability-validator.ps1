[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$powerShell = (Get-Process -Id $PID).Path
$fixtureRoot = Join-Path (
    [System.IO.Path]::GetTempPath()
) ("wpm-traceability-validator-{0}" -f [guid]::NewGuid().ToString('N'))

function Invoke-Validator {
    param(
        [Parameter(Mandatory)]
        [string]$Root,
        [string]$ReleaseBaseline = '',
        [string]$RunnerPlanId = '',
        [switch]$SkipRunnerPlanExecution
    )

    $arguments = @(
        '-NoProfile',
        '-ExecutionPolicy',
        'Bypass',
        '-File',
        (Join-Path $Root 'tests/verify-traceability.ps1'),
        '-RepositoryRoot',
        $Root
    )
    if ($ReleaseBaseline) {
        $arguments += @('-ReleaseBaseline', $ReleaseBaseline)
    }
    if ($RunnerPlanId) {
        $arguments += @('-RunnerPlanId', $RunnerPlanId)
    }
    if ($SkipRunnerPlanExecution) {
        $arguments += '-SkipRunnerPlanExecution'
    }
    $output = & $powerShell @arguments 2>&1 | Out-String
    [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output = $output
    }
}

function Assert-FailedWith {
    param(
        [Parameter(Mandatory)]
        $Result,
        [Parameter(Mandatory)]
        [string]$Pattern
    )

    if ($Result.ExitCode -eq 0) {
        throw "Traceability validation unexpectedly passed. Output: $($Result.Output)"
    }
    if ($Result.Output -notmatch $Pattern) {
        throw "Traceability validation did not report '$Pattern'. Output: $($Result.Output)"
    }
}

try {
    $positive = Invoke-Validator -Root $RepositoryRoot
    if ($positive.ExitCode -ne 0) {
        throw "The controlled repository failed positive validation. $($positive.Output)"
    }

    $releaseGate = Invoke-Validator -Root $RepositoryRoot -ReleaseBaseline '2.0' `
        -SkipRunnerPlanExecution
    Assert-FailedWith -Result $releaseGate -Pattern 'not Verified for the 2\.0 release baseline'

    New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
    Copy-Item -Recurse -LiteralPath (Join-Path $RepositoryRoot 'docs') -Destination $fixtureRoot
    Copy-Item -Recurse -LiteralPath (Join-Path $RepositoryRoot 'tests') -Destination $fixtureRoot
    New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'wpm') | Out-Null
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'wpm/CMakeLists.txt') -Destination (Join-Path $fixtureRoot 'wpm/CMakeLists.txt')

    $matrixPath = Join-Path $fixtureRoot 'docs/traceability-2.0.md'
    $controlledMatrix = Get-Content -Raw -LiteralPath $matrixPath

    $missingRow = $controlledMatrix -replace '(?m)^\| REQ-0014\.001 .+\r?\n', ''
    Set-Content -NoNewline -LiteralPath $matrixPath -Value $missingRow
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -SkipRunnerPlanExecution) -Pattern 'REQ-0014\.001 must have exactly one'

    $duplicateRow = [regex]::Match(
        $controlledMatrix,
        '(?m)^\| REQ-0014\.001 .+$'
    ).Value
    Set-Content -NoNewline -LiteralPath $matrixPath -Value ($controlledMatrix + "`n" + $duplicateRow + "`n")
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -SkipRunnerPlanExecution) -Pattern 'Duplicate WPM 2\.0 traceability rows: REQ-0014\.001'

    $verifiedWithoutEvidence = $controlledMatrix -replace (
        '\| REQ-0014\.001 \| TC-0014 \| Automated test and inspection \| Planned \| Not yet produced \|'
    ), '| REQ-0014.001 | TC-0014 | Automated test and inspection | Verified | Not yet produced |'
    Set-Content -NoNewline -LiteralPath $matrixPath -Value $verifiedWithoutEvidence
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -SkipRunnerPlanExecution) -Pattern 'REQ-0014\.001 is Verified without objective evidence'

    Set-Content -NoNewline -LiteralPath $matrixPath -Value $controlledMatrix
    $specPath = Join-Path $fixtureRoot 'docs/tc-0014-command-output-and-help.tex'
    Remove-Item -LiteralPath $specPath
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -SkipRunnerPlanExecution) `
        -Pattern 'REQ-0014 must have exactly one tc-0014 test specification'
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'docs/tc-0014-command-output-and-help.tex') `
        -Destination $specPath

    $runnerPath = Join-Path $fixtureRoot 'tests/tc-0014-command-output-and-help.ps1'
    Remove-Item -LiteralPath $runnerPath
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -SkipRunnerPlanExecution) `
        -Pattern 'REQ-0014 must have exactly one tc-0014 automated test'
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'tests/tc-0014-command-output-and-help.ps1') `
        -Destination $runnerPath

    $controlledRunner = Get-Content -Raw -LiteralPath $runnerPath
    $missingAllocation = $controlledRunner -replace `
        "(?m)^\s*@\{ Requirement = 'REQ-0014\.001'.*\r?\n", ''
    Set-Content -NoNewline -LiteralPath $runnerPath -Value $missingAllocation
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -RunnerPlanId '0014') `
        -Pattern 'TC-0014 runner plan must allocate REQ-0014\.001 exactly once'

    $missingExpected = $controlledRunner -replace `
        "Expected = 'Every public help path", "ExpectedOmitted = 'Every public help path"
    Set-Content -NoNewline -LiteralPath $runnerPath -Value $missingExpected
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -RunnerPlanId '0014') `
        -Pattern 'TC-0014 runner case REQ-0014\.001 has no objective expected result'

    $libraryPath = Join-Path $fixtureRoot 'tests/wpm-2.0-planned-test-lib.ps1'
    $controlledLibrary = Get-Content -Raw -LiteralPath $libraryPath
    $missingProfile = $controlledRunner -replace `
        'if \(\$Describe\)', `
        ('$null = $plan.Profiles.Remove(''ManualRealEnvironment'')' + "`r`n" + 'if ($Describe)')
    Set-Content -NoNewline -LiteralPath $runnerPath -Value $missingProfile
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -RunnerPlanId '0014') `
        -Pattern 'TC-0014 runner description is missing the ManualRealEnvironment profile'

    Set-Content -NoNewline -LiteralPath $runnerPath -Value $controlledRunner
    $missingEvidencePath = $controlledLibrary -replace `
        'EvidencePath = "Testing/Evidence', 'EvidencePathOmitted = "Testing/Evidence'
    Set-Content -NoNewline -LiteralPath $libraryPath -Value $missingEvidencePath
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -RunnerPlanId '0014') `
        -Pattern 'TC-0014 Fast profile has no evidence path'

    $missingGate = $controlledLibrary -replace `
        'Gate = \$profileDefinitions\[\$profileName\]', `
        'GateOmitted = $profileDefinitions[$profileName]'
    Set-Content -NoNewline -LiteralPath $libraryPath -Value $missingGate
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot -RunnerPlanId '0014') `
        -Pattern 'TC-0014 Fast profile has no release-gate allocation'

    Write-Host 'Traceability validator positive, release-gate, trace-row, evidence-state, planned-artifact, allocation, profile, expected-result, evidence-path, and gate tests passed.'
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -Recurse -Force -LiteralPath $fixtureRoot
    }
}

# Expected negative child validations leave LASTEXITCODE set to 1. Reaching
# this point means every assertion and fixture cleanup succeeded; reset the
# caller-visible native status without terminating a host that invoked us.
$global:LASTEXITCODE = 0
