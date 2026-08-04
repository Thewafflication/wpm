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
        [string]$ReleaseBaseline = ''
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

    $releaseGate = Invoke-Validator -Root $RepositoryRoot -ReleaseBaseline '2.0'
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
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot) -Pattern 'REQ-0014\.001 must have exactly one'

    $duplicateRow = [regex]::Match(
        $controlledMatrix,
        '(?m)^\| REQ-0014\.001 .+$'
    ).Value
    Set-Content -NoNewline -LiteralPath $matrixPath -Value ($controlledMatrix + "`n" + $duplicateRow + "`n")
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot) -Pattern 'Duplicate WPM 2\.0 traceability rows: REQ-0014\.001'

    $verifiedWithoutEvidence = $controlledMatrix -replace (
        '\| REQ-0014\.001 \| TC-0014 \| Automated test and inspection \| Planned \| Not yet produced \|'
    ), '| REQ-0014.001 | TC-0014 | Automated test and inspection | Verified | Not yet produced |'
    Set-Content -NoNewline -LiteralPath $matrixPath -Value $verifiedWithoutEvidence
    Assert-FailedWith -Result (Invoke-Validator -Root $fixtureRoot) -Pattern 'REQ-0014\.001 is Verified without objective evidence'

    Write-Host 'Traceability validator positive, release-gate, missing-row, duplicate-row, and evidence-state tests passed.'
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -Recurse -Force -LiteralPath $fixtureRoot
    }
}
