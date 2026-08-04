param([switch]$Describe,
      [ValidateSet('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')]
      [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases=@(
    @{Requirement='REQ-0017.001';Technique='state-transition';Profile='Fast';Expected='Initialization is deterministic and never overwrites unapproved state.'},
    @{Requirement='REQ-0017.002';Technique='cause-effect/fault injection';Profile='Quality';Expected='Validation precedes an atomic package copy and failure leaves no valid-looking partial.'},
    @{Requirement='REQ-0017.003';Technique='combinatorial/determinism';Profile='Fast';Expected='Index output is deterministic and atomic; invalid identity/path combinations fail.'},
    @{Requirement='REQ-0017.004';Technique='decision-table/security';Profile='ManualRealEnvironment';Expected='Repository-scoped signing works with protected keys and emits no key material.'},
    @{Requirement='REQ-0017.005';Technique='classification-tree';Profile='Fast';Expected='Read-only verification reports all independent findings and never mutates.'},
    @{Requirement='REQ-0017.006';Technique='state-difference';Profile='Fast';Expected='Plans and dry runs match shared contracts; verification remains read-only.'},
    @{Requirement='REQ-0017.007';Technique='use-case';Profile='PlatformMatrix';Expected='A copied read-only tree remains locator-neutral and consumable.'},
    @{Requirement='REQ-0017.008';Technique='scenario testing';Profile='ReleaseGate';Expected='The documented initialize-to-install journey is reproducible.'}
)
$rationales=@{
    Fast='Disposable repositories cover deterministic creation, indexing, validation, plans, and no-mutation checks.'
    PlatformMatrix='Native architectures cover filesystem atomicity, permissions, and read-only copied trees.'
    Quality='Full disk, interruption, races, reparse points, hostile archives, and large repositories are bounded.'
    ManualRealEnvironment='Protected-key signing and mastered USB/optical media require controlled operator evidence.'
    ReleaseGate='Round-trip author, sign, verify, copy, configure, update, and install evidence must pass.'
}
$plan=New-Wpm20PlannedTestPlan 'TC-0017' 'REQ-0017' 'Repository authoring' $cases $rationales @('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')
if($Describe){Write-Wpm20PlannedTestPlan $plan;exit 0}
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
