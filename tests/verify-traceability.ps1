[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$docs = Join-Path $RepositoryRoot 'docs'
$cmake = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'wpm/CMakeLists.txt')
$failures = [System.Collections.Generic.List[string]]::new()
$allRequirementIds = [System.Collections.Generic.List[string]]::new()
$requiredTestCaseFields = @(
    'TCID',
    'TCTitle',
    'TCPurpose',
    'TCPriority',
    'TCRequirementRef',
    'TCDesignRef',
    'TCFeatureRef',
    'TCTestTechnique',
    'TCPreconditions',
    'TCEnvironment',
    'TCAssumptions',
    'TCInputData',
    'TCInitialState',
    'TCProcedure',
    'TCExpectedResult',
    'TCPostConditions'
)

$requirements = Get-ChildItem -LiteralPath $docs -Filter 'req-*.md' |
    ForEach-Object { if ($_.BaseName -match '^req-(\d{4})') { $Matches[1] } } |
    Sort-Object -Unique

foreach ($id in $requirements) {
    $testId = "TC-$id"
    $slug = "tc-$id"
    $requirementFile = Get-ChildItem -LiteralPath $docs -Filter "req-$id-*.md"
    if ($requirementFile.Count -ne 1) {
        $failures.Add("REQ-$id must have exactly one requirement document.")
        continue
    }
    $requirement = Get-Content -Raw -LiteralPath $requirementFile.FullName
    $tex = Get-ChildItem -LiteralPath $docs -Filter "$slug-*.tex"
    $script = Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'tests') -Filter "$slug-*.ps1"

    if ($requirement -notmatch [regex]::Escape($testId)) { $failures.Add("REQ-$id does not reference $testId.") }
    foreach ($pattern in @(
        '(?m)^\*\*Content type:\*\* Project requirements\r?$',
        '(?m)^\*\*Status:\*\* (Proposed|Accepted|Deprecated|Superseded|Rejected)',
        '(?m)^\*\*Source:\*\* .+\r?$',
        '(?m)^## Scope\r?$',
        '(?m)^## Requirement\r?$',
        '(?m)^## Rationale\r?$',
        '(?m)^## Verification\r?$',
        '(?m)^\*\*Method:\*\* .+',
        '(?m)^\*\*References:\*\* .+',
        '(?m)^## Relationships\r?$',
        '(?m)^## Tailoring\r?$',
        '(?m)^## Implementation Record\r?$'
    )) {
        if ($requirement -notmatch $pattern) {
            $failures.Add("REQ-$id is missing required WSP structure matching: $pattern")
        }
    }

    $subordinateIds = [regex]::Matches(
        $requirement,
        "(?m)^\*\*(REQ-$id\.\d{3})\*\*\r?$"
    ) | ForEach-Object { $_.Groups[1].Value }
    foreach ($subordinateId in $subordinateIds) {
        $allRequirementIds.Add($subordinateId)
    }
    if ($subordinateIds.Count -eq 0) {
        $failures.Add("REQ-$id has no identified subordinate obligations.")
    }

    $requirementSection = [regex]::Match(
        $requirement,
        '(?s)(?<=## Requirement\r?\n).*?(?=\r?\n## (?:Rationale|Verification))'
    ).Value
    foreach ($block in [regex]::Split($requirementSection, '(?:\r?\n){2,}')) {
        if ($block -match '(?i)\b(shall|must)\b' -and
            $block -notmatch "(?m)^\*\*REQ-$id\.\d{3}\*\*\r?$") {
            $failures.Add("REQ-$id contains an unidentified normative obligation: $($block.Substring(0, [Math]::Min(80, $block.Length)))")
        }
    }
    if ($tex.Count -ne 1) { $failures.Add("REQ-$id must have exactly one $slug test specification.") }
    if ($script.Count -ne 1) { $failures.Add("REQ-$id must have exactly one $slug automated test.") }
    if ($tex.Count -eq 1) {
        $contents = Get-Content -Raw -LiteralPath $tex.FullName
        if ($contents -notmatch [regex]::Escape("REQ-$id")) { $failures.Add("$($tex.Name) does not reference REQ-$id.") }
        if ($contents -notmatch [regex]::Escape('\input{wsp/testing/test-case-library.tex}')) {
            $failures.Add("$($tex.Name) does not use the pinned WSP test-case library.")
        }
        foreach ($field in $requiredTestCaseFields) {
            $definition = "\\def\\$field\s*\{(?s:.+?)\}"
            if ($contents -notmatch $definition) {
                $failures.Add("$($tex.Name) does not define a non-empty $field field.")
            }
        }
    }
    if ($cmake -notmatch [regex]::Escape("wpm_add_test_case($testId")) { $failures.Add("$testId is not registered with CTest.") }
}

$duplicateRequirementIds = $allRequirementIds |
    Group-Object |
    Where-Object Count -gt 1 |
    ForEach-Object Name
if ($duplicateRequirementIds) {
    $failures.Add("Duplicate subordinate requirement identifiers: $($duplicateRequirementIds -join ', ')")
}

if ($failures.Count) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Host "Traceability verified for $($requirements.Count) requirements and test cases."
