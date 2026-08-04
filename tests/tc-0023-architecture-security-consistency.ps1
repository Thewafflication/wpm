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
$expectedWspCommit = '2198ccab08f969a789448767fe7017b774369adc'
$started = Get-Date
$results = @()

$controlledPaths = @(
    'docs/adr-0010-command-events-and-machine-readable-output.md',
    'docs/adr-0011-transport-neutral-repository-access.md',
    'docs/adr-0012-repository-authoring-and-index-signing.md',
    'docs/adr-0013-operation-plans-dry-run-and-recovery.md',
    'docs/change-impact-2.0-architecture-and-security.md',
    'docs/dfs.md',
    'docs/req-0014-command-output-and-help.md',
    'docs/req-0015-safer-package-changes.md',
    'docs/req-0016-repository-transports.md',
    'docs/req-0017-repository-authoring.md',
    'docs/req-0018-discoverability-and-diagnostics.md',
    'docs/req-0019-recovery-and-lifecycle.md',
    'docs/req-0023-architecture-and-security-consistency.md',
    'docs/traceability-2.0.md',
    'documentation/documentation-manifest.json'
)

function Get-ControlledText {
    $text = @{}
    foreach ($relativePath in $controlledPaths) {
        $path = Join-Path $repositoryRoot $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Missing controlled artifact: $relativePath"
        }
        $text[$relativePath] = Get-Content -Raw -LiteralPath $path
    }
    return $text
}

function Assert-Contains {
    param(
        [hashtable]$Text,
        [string]$Path,
        [string]$Pattern,
        [string]$Description
    )

    if ($Text[$Path] -notmatch $Pattern) {
        throw "$Path is missing $Description."
    }
}

function Assert-ArchitectureBaseline {
    param(
        [hashtable]$Text,
        [string]$WspCommit
    )

    $impact = 'docs/change-impact-2.0-architecture-and-security.md'
    foreach ($id in 1..9) {
        Assert-Contains -Text $Text -Path $impact `
            -Pattern ("(?m)^- \*\*ADR-{0:D4} " -f $id) `
            -Description ("the ADR-{0:D4} review disposition" -f $id)
    }

    $adrExpectations = @{
        'docs/adr-0010-command-events-and-machine-readable-output.md' = @(
            'Supplements ADR-0007', '## Explicit Non-Goals', 'REQ-0014', 'REQ-0018'
        )
        'docs/adr-0011-transport-neutral-repository-access.md' = @(
            'Supersedes ADR-0005', '## Explicit Non-Goals', 'REQ-0016'
        )
        'docs/adr-0012-repository-authoring-and-index-signing.md' = @(
            'Supplements ADR-0002', '## Explicit Non-Goals', 'REQ-0017'
        )
        'docs/adr-0013-operation-plans-dry-run-and-recovery.md' = @(
            'Supplements ADR-0003', '## Explicit Non-Goals', 'REQ-0015', 'REQ-0019'
        )
    }
    foreach ($entry in $adrExpectations.GetEnumerator()) {
        Assert-Contains -Text $Text -Path $entry.Key `
            -Pattern '(?m)^\*\*Status:\*\* Accepted$' `
            -Description 'Accepted status'
        foreach ($literal in $entry.Value) {
            Assert-Contains -Text $Text -Path $entry.Key `
                -Pattern ([regex]::Escape($literal)) `
                -Description $literal
        }
    }

    $requirementReferences = @{
        'docs/req-0014-command-output-and-help.md' = 'ADR-0010'
        'docs/req-0015-safer-package-changes.md' = 'ADR-0013'
        'docs/req-0016-repository-transports.md' = 'ADR-0011'
        'docs/req-0017-repository-authoring.md' = 'ADR-0012'
        'docs/req-0018-discoverability-and-diagnostics.md' = 'ADR-0010'
        'docs/req-0019-recovery-and-lifecycle.md' = 'ADR-0013'
    }
    foreach ($entry in $requirementReferences.GetEnumerator()) {
        Assert-Contains -Text $Text -Path $entry.Key `
            -Pattern ([regex]::Escape($entry.Value)) `
            -Description ("the governing {0} reference" -f $entry.Value)
    }
    Assert-Contains -Text $Text -Path 'docs/req-0016-repository-transports.md' `
        -Pattern 'Supersedes for WPM 2\.0' `
        -Description 'the scoped REQ-0011 supersession'

    $dfs = 'docs/dfs.md'
    foreach ($id in 13..20) {
        Assert-Contains -Text $Text -Path $dfs `
            -Pattern ("WPM-THR-{0:D3}" -f $id) `
            -Description ("WPM-THR-{0:D3}" -f $id)
    }
    foreach ($term in @(
        'Repository index-signing keys and authorization',
        'Command and machine output',
        'Cleanup classifications and allowed roots',
        'Locator to repository reader',
        'Private authoring key to index signature',
        'Resolved plan to mutation',
        'Recovery record to retry',
        'Cleanup inventory to deletion',
        'wpm.recovery.v1',
        'TC-0023 statically verifies',
        'Opted-in HTTP has no confidentiality'
    )) {
        Assert-Contains -Text $Text -Path $dfs -Pattern ([regex]::Escape($term)) `
            -Description $term
    }

    $manifest = 'documentation/documentation-manifest.json'
    foreach ($relativePath in @(
        'docs/change-impact-2.0-architecture-and-security.md',
        'docs/adr-0010-command-events-and-machine-readable-output.md',
        'docs/adr-0011-transport-neutral-repository-access.md',
        'docs/adr-0012-repository-authoring-and-index-signing.md',
        'docs/adr-0013-operation-plans-dry-run-and-recovery.md',
        'docs/req-0023-architecture-and-security-consistency.md',
        'docs/tc-0023-architecture-security-consistency.tex'
    )) {
        Assert-Contains -Text $Text -Path $manifest `
            -Pattern ([regex]::Escape('"' + $relativePath + '"')) `
            -Description ("the $relativePath manifest entry")
    }

    Assert-Contains -Text $Text -Path 'docs/traceability-2.0.md' `
        -Pattern '(?m)^\| REQ-0023\.005 \| TC-0023 \|' `
        -Description 'REQ-0023 bidirectional traceability'

    if ($WspCommit -ne $expectedWspCommit) {
        throw "The wsp gitlink is $WspCommit, expected $expectedWspCommit."
    }
}

function Invoke-NegativeFixture {
    param(
        [hashtable]$ControlledText,
        [string]$Path,
        [string]$Pattern,
        [string]$ExpectedFailure,
        [string]$WspCommit = $expectedWspCommit
    )

    $fixture = @{}
    foreach ($key in $ControlledText.Keys) { $fixture[$key] = $ControlledText[$key] }
    if ($Path) { $fixture[$Path] = $fixture[$Path] -replace $Pattern, '' }

    try {
        Assert-ArchitectureBaseline -Text $fixture -WspCommit $WspCommit
    }
    catch {
        if ($_.Exception.Message -notmatch $ExpectedFailure) {
            throw "Fixture failed for an unexpected reason: $($_.Exception.Message)"
        }
        return "Rejected as expected: $($_.Exception.Message)"
    }
    throw 'The negative fixture unexpectedly passed.'
}

$controlledText = Get-ControlledText
$gitLinkLine = (& git -C $repositoryRoot ls-files -s wsp 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $gitLinkLine -notmatch '^160000 ([0-9a-f]{40}) 0\s+wsp$') {
    throw "Unable to read the wsp gitlink without submodule execution: $gitLinkLine"
}
$actualWspCommit = $Matches[1]
$wspStatus = (& git -C $repositoryRoot status --short -- wsp 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0) { throw "Unable to inspect wsp status: $wspStatus" }
if ($wspStatus) { throw 'The pinned wsp gitlink or worktree is modified.' }

$results += New-WpmManualStep -Name 'Validate the controlled architecture and DFS baseline' -Action {
    Assert-ArchitectureBaseline -Text $controlledText -WspCommit $actualWspCommit
    'Accepted ADR dispositions, relationships, DFS coverage, manifest, traceability, and WSP pin are consistent.'
}
$results += New-WpmManualStep -Name 'Reject a missing accepted-ADR disposition' -Action {
    Invoke-NegativeFixture -ControlledText $controlledText `
        -Path 'docs/change-impact-2.0-architecture-and-security.md' `
        -Pattern '(?ms)^- \*\*ADR-0009 .*?(?=\r?\n\r?\n|\z)' `
        -ExpectedFailure 'ADR-0009 review disposition'
}
$results += New-WpmManualStep -Name 'Reject a missing explicit non-goal section' -Action {
    Invoke-NegativeFixture -ControlledText $controlledText `
        -Path 'docs/adr-0010-command-events-and-machine-readable-output.md' `
        -Pattern '## Explicit Non-Goals' -ExpectedFailure 'Explicit Non-Goals'
}
$results += New-WpmManualStep -Name 'Reject a missing requirement-to-ADR reference' -Action {
    Invoke-NegativeFixture -ControlledText $controlledText `
        -Path 'docs/req-0014-command-output-and-help.md' `
        -Pattern 'ADR-0010' -ExpectedFailure 'governing ADR-0010 reference'
}
$results += New-WpmManualStep -Name 'Reject missing DFS cleanup-threat coverage' -Action {
    Invoke-NegativeFixture -ControlledText $controlledText -Path 'docs/dfs.md' `
        -Pattern 'WPM-THR-020' -ExpectedFailure 'WPM-THR-020'
}
$results += New-WpmManualStep -Name 'Reject a missing documentation-manifest entry' -Action {
    Invoke-NegativeFixture -ControlledText $controlledText `
        -Path 'documentation/documentation-manifest.json' `
        -Pattern '"docs/adr-0013-operation-plans-dry-run-and-recovery\.md",?\r?\n' `
        -ExpectedFailure 'adr-0013-operation-plans-dry-run-and-recovery'
}
$results += New-WpmManualStep -Name 'Reject a changed WSP baseline identity' -Action {
    Invoke-NegativeFixture -ControlledText $controlledText -Path '' -Pattern '' `
        -ExpectedFailure 'wsp gitlink' -WspCommit ('0' * 40)
}

$finished = Get-Date
if ($EvidenceTex) {
    Write-WpmTestEvidence -TestCaseId 'TC-0023' -WpmExe $WpmExe -Started $started `
        -Finished $finished -Results $results -EvidenceTex $EvidenceTex
}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
