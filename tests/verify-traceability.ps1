[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [ValidateSet('', '2.0')]
    [string]$ReleaseBaseline = '',
    [ValidatePattern('^\d{4}$')]
    [string]$RunnerPlanId = '',
    [switch]$SkipRunnerPlanExecution
)

$ErrorActionPreference = 'Stop'
$docs = Join-Path $RepositoryRoot 'docs'
$cmake = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'wpm/CMakeLists.txt')
$powerShell = (Get-Process -Id $PID).Path
$failures = [System.Collections.Generic.List[string]]::new()
$allRequirementIds = [System.Collections.Generic.List[string]]::new()
$requirementRecords = @{}
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

$requirementFiles = Get-ChildItem -LiteralPath $docs -Filter 'req-*.md' |
    Where-Object BaseName -Match '^req-\d{4}(?:-|$)' |
    Sort-Object Name
$requirements = $requirementFiles |
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
    $statusMatch = [regex]::Match(
        $requirement,
        '(?m)^\*\*Status:\*\* (Proposed|Accepted|Deprecated|Superseded|Rejected)\r?$'
    )
    $requirementStatus = if ($statusMatch.Success) {
        $statusMatch.Groups[1].Value
    } else {
        ''
    }
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
    $requirementRecords["REQ-$id"] = [pscustomobject]@{
        Id = "REQ-$id"
        NumericId = $id
        Status = $requirementStatus
        File = $requirementFile.Name
        SubordinateIds = @($subordinateIds)
        TestId = $testId
        TestSpecificationCount = $tex.Count
        RunnerCount = $script.Count
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
    $requiresImplementedTest = $requirementStatus -eq 'Accepted'
    $requiresControlled20Plan = [int]$id -ge 14
    if (($requiresImplementedTest -or $requiresControlled20Plan) -and $tex.Count -ne 1) {
        $failures.Add("REQ-$id must have exactly one $slug test specification.")
    }
    if (($requiresImplementedTest -or $requiresControlled20Plan) -and $script.Count -ne 1) {
        $failures.Add("REQ-$id must have exactly one $slug automated test.")
    }
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
    if ($requiresImplementedTest -and
        $cmake -notmatch [regex]::Escape("wpm_add_test_case($testId")) {
        $failures.Add("$testId is not registered with CTest.")
    }
    if ($requiresControlled20Plan -and $requirementStatus -eq 'Proposed' -and
        $cmake -match [regex]::Escape("wpm_add_test_case($testId")) {
        $failures.Add("$testId is Proposed and must not be registered as passing product verification.")
    }

    if ($requiresControlled20Plan -and $script.Count -eq 1 -and
        -not $SkipRunnerPlanExecution -and
        (-not $RunnerPlanId -or $RunnerPlanId -eq $id)) {
        $describeOutput = & $powerShell -NoProfile -ExecutionPolicy Bypass `
            -File $script.FullName -Describe 2>&1 | Out-String
        $describeExitCode = $LASTEXITCODE
        if ($describeExitCode -ne 0) {
            $failures.Add("$testId runner description failed: $($describeOutput.Trim())")
        } else {
            try {
                $plan = $describeOutput | ConvertFrom-Json
            } catch {
                $plan = $null
                $failures.Add("$testId runner description is not valid JSON.")
            }
            if ($null -ne $plan) {
                if ($plan.Schema -ne 'wpm.test-plan.v1') {
                    $failures.Add("$testId runner description has no wpm.test-plan.v1 schema.")
                }
                if ($plan.TestCaseId -ne $testId -or $plan.RequirementId -ne "REQ-$id") {
                    $failures.Add("$testId runner description has inconsistent requirement or test identity.")
                }
                $expectedExecutionState = if ($requirementStatus -eq 'Proposed') { 'Planned' } else { 'Implemented' }
                if ($plan.ExecutionState -ne $expectedExecutionState) {
                    $failures.Add("$testId runner description must report $expectedExecutionState, not $($plan.ExecutionState).")
                }
                $profileNames = @('Fast', 'PlatformMatrix', 'Quality', 'ManualRealEnvironment', 'ReleaseGate')
                foreach ($profileName in $profileNames) {
                    $profile = $plan.Profiles.$profileName
                    if ($null -eq $profile) {
                        $failures.Add("$testId runner description is missing the $profileName profile.")
                        continue
                    }
                    if ([string]::IsNullOrWhiteSpace([string]$profile.Rationale)) {
                        $failures.Add("$testId $profileName profile has no rationale.")
                    }
                    if ([string]::IsNullOrWhiteSpace([string]$profile.EvidencePath)) {
                        $failures.Add("$testId $profileName profile has no evidence path.")
                    }
                    if ([string]::IsNullOrWhiteSpace([string]$profile.Gate)) {
                        $failures.Add("$testId $profileName profile has no release-gate allocation.")
                    }
                }
                $caseRequirementIds = @($plan.Cases | ForEach-Object Requirement)
                foreach ($subordinateId in $subordinateIds) {
                    if (@($caseRequirementIds | Where-Object { $_ -eq $subordinateId }).Count -ne 1) {
                        $failures.Add("$testId runner plan must allocate $subordinateId exactly once.")
                    }
                }
                foreach ($case in @($plan.Cases)) {
                    if ($case.Requirement -notin $subordinateIds) {
                        $failures.Add("$testId runner plan references unknown $($case.Requirement).")
                    }
                    if ([string]::IsNullOrWhiteSpace([string]$case.Technique)) {
                        $failures.Add("$testId runner case $($case.Requirement) has no test-design technique.")
                    }
                    if ([string]::IsNullOrWhiteSpace([string]$case.Expected)) {
                        $failures.Add("$testId runner case $($case.Requirement) has no objective expected result.")
                    }
                    if ($case.Profile -notin $profileNames) {
                        $failures.Add("$testId runner case $($case.Requirement) has unknown profile $($case.Profile).")
                    }
                }
            }
        }

        if ($requirementStatus -eq 'Proposed') {
            $blockedOutput = & $powerShell -NoProfile -ExecutionPolicy Bypass `
                -File $script.FullName -ExecutionProfile Fast 2>&1 | Out-String
            $blockedExitCode = $LASTEXITCODE
            try { $blockedResult = $blockedOutput | ConvertFrom-Json } catch { $blockedResult = $null }
            if ($blockedExitCode -ne 2 -or $null -eq $blockedResult -or
                $blockedResult.Status -ne 'Blocked' -or
                $blockedResult.TestCaseId -ne $testId) {
                $failures.Add("$testId Proposed runner must return controlled Blocked status and exit 2 without producing evidence.")
            }
        }
    }
}

$duplicateRequirementIds = $allRequirementIds |
    Group-Object |
    Where-Object Count -gt 1 |
    ForEach-Object Name
if ($duplicateRequirementIds) {
    $failures.Add("Duplicate subordinate requirement identifiers: $($duplicateRequirementIds -join ', ')")
}

$traceability10Path = Join-Path $docs 'traceability-1.0.md'
$traceability20Path = Join-Path $docs 'traceability-2.0.md'
if (-not (Test-Path -LiteralPath $traceability10Path)) {
    $failures.Add('The WPM 1.0 traceability matrix is missing.')
}
if (-not (Test-Path -LiteralPath $traceability20Path)) {
    $failures.Add('The WPM 2.0 traceability matrix is missing.')
}

$traceRows10 = @()
if (Test-Path -LiteralPath $traceability10Path) {
    $traceability10 = Get-Content -Raw -LiteralPath $traceability10Path
    $traceRows10 = [regex]::Matches(
        $traceability10,
        '(?m)^\| (REQ-(\d{4})) \| (TC-(\d{4})) \| .+ \|\r?$'
    ) | ForEach-Object {
        [pscustomobject]@{
            RequirementId = $_.Groups[1].Value
            RequirementNumericId = $_.Groups[2].Value
            TestId = $_.Groups[3].Value
            TestNumericId = $_.Groups[4].Value
        }
    }
}

$traceRows20 = @()
if (Test-Path -LiteralPath $traceability20Path) {
    $traceability20 = Get-Content -Raw -LiteralPath $traceability20Path
    $traceRows20 = [regex]::Matches(
        $traceability20,
        '(?m)^\| (REQ-(\d{4})\.\d{3}) \| (TC-(\d{4})) \| ([^|]+) \| (Planned|Verified) \| ([^|]+) \|\r?$'
    ) | ForEach-Object {
        [pscustomobject]@{
            RequirementId = $_.Groups[1].Value
            RequirementNumericId = $_.Groups[2].Value
            TestId = $_.Groups[3].Value
            TestNumericId = $_.Groups[4].Value
            Method = $_.Groups[5].Value.Trim()
            State = $_.Groups[6].Value
            Evidence = $_.Groups[7].Value.Trim()
        }
    }
}

foreach ($row in $traceRows10) {
    if (-not $requirementRecords.ContainsKey($row.RequirementId)) {
        $failures.Add("The 1.0 traceability matrix references missing $($row.RequirementId).")
    }
    if ($row.RequirementNumericId -ne $row.TestNumericId) {
        $failures.Add("$($row.RequirementId) is allocated to mismatched $($row.TestId).")
    }
}

$duplicateRows20 = $traceRows20 |
    Group-Object RequirementId |
    Where-Object Count -gt 1 |
    ForEach-Object Name
if ($duplicateRows20) {
    $failures.Add("Duplicate WPM 2.0 traceability rows: $($duplicateRows20 -join ', ')")
}

$planned20Requirements = $requirementRecords.Values |
    Where-Object { [int]$_.NumericId -ge 14 } |
    Sort-Object NumericId
$expected20Subordinates = @($planned20Requirements | ForEach-Object SubordinateIds)
foreach ($subordinateId in $expected20Subordinates) {
    $matches = @($traceRows20 | Where-Object RequirementId -eq $subordinateId)
    if ($matches.Count -ne 1) {
        $failures.Add("$subordinateId must have exactly one WPM 2.0 traceability row.")
        continue
    }
    $parentId = $subordinateId.Substring(0, 8)
    $expectedTestId = 'TC-' + $subordinateId.Substring(4, 4)
    if ($matches[0].TestId -ne $expectedTestId) {
        $failures.Add("$subordinateId must be allocated to $expectedTestId, not $($matches[0].TestId).")
    }
    if ([string]::IsNullOrWhiteSpace($matches[0].Method)) {
        $failures.Add("$subordinateId has no planned verification method.")
    }
    if (-not $requirementRecords.ContainsKey($parentId)) {
        $failures.Add("$subordinateId has no requirement document.")
    }
}

foreach ($row in $traceRows20) {
    if ($row.RequirementId -notin $expected20Subordinates) {
        $failures.Add("The WPM 2.0 traceability matrix references unknown $($row.RequirementId).")
    }
    if ($row.State -eq 'Verified' -and
        ($row.Evidence -eq 'Not yet produced' -or
         [string]::IsNullOrWhiteSpace($row.Evidence))) {
        $failures.Add("$($row.RequirementId) is Verified without objective evidence.")
    }
}

if ($ReleaseBaseline -eq '2.0') {
    foreach ($requirementRecord in $planned20Requirements) {
        if ($requirementRecord.Status -ne 'Accepted') {
            $failures.Add("$($requirementRecord.Id) is $($requirementRecord.Status), not Accepted for the 2.0 release baseline.")
        }
        if ($requirementRecord.TestSpecificationCount -ne 1) {
            $failures.Add("$($requirementRecord.Id) lacks exactly one controlled $($requirementRecord.TestId) specification for the 2.0 release baseline.")
        }
        if ($requirementRecord.RunnerCount -ne 1) {
            $failures.Add("$($requirementRecord.Id) lacks exactly one automated $($requirementRecord.TestId) runner for the 2.0 release baseline.")
        }
        if ($cmake -notmatch [regex]::Escape("wpm_add_test_case($($requirementRecord.TestId)")) {
            $failures.Add("$($requirementRecord.TestId) is not registered with CTest for the 2.0 release baseline.")
        }
    }
    foreach ($row in $traceRows20) {
        if ($row.State -ne 'Verified') {
            $failures.Add("$($row.RequirementId) is not Verified for the 2.0 release baseline.")
        }
        if ($row.Evidence -eq 'Not yet produced' -or
            [string]::IsNullOrWhiteSpace($row.Evidence)) {
            $failures.Add("$($row.RequirementId) has no objective 2.0 release evidence.")
        }
    }
}

if ($failures.Count) {
    $failures | ForEach-Object { [Console]::Error.WriteLine($_) }
    exit 1
}

$summary = (
    "Traceability verified for {0} requirements, {1} identified obligations, " +
    "and {2} WPM 2.0 verification allocations."
) -f
    $requirements.Count,
    $allRequirementIds.Count,
    $traceRows20.Count
Write-Host $summary
