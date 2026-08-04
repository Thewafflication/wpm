param([switch]$Describe,
      [ValidateSet('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')]
      [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases=@(
    @{Requirement='REQ-0018.001';Technique='classification-tree';Profile='Fast';Expected='Healthy and degraded diagnosis covers every required state without mutation.'},
    @{Requirement='REQ-0018.002';Technique='decision-table';Profile='Fast';Expected='Each unhealthy condition maps to an actionable safe remediation.'},
    @{Requirement='REQ-0018.003';Technique='equivalence-partitioning';Profile='Fast';Expected='Installed identities and corrupt/ambiguous records are distinguished without extraction.'},
    @{Requirement='REQ-0018.004';Technique='decision-table';Profile='Fast';Expected='Available results apply policy and identify fresh versus stale data without acquisition.'},
    @{Requirement='REQ-0018.005';Technique='combinatorial';Profile='Quality';Expected='Filter combinations are deterministic and invalid filters never broaden results.'},
    @{Requirement='REQ-0018.006';Technique='use-case';Profile='Fast';Expected='Show reports complete package, source, trust, upgrade, audit, and recovery context read-only.'},
    @{Requirement='REQ-0018.007';Technique='equivalence-partitioning';Profile='Fast';Expected='Narrowed and ambiguous selections return documented results.'},
    @{Requirement='REQ-0018.008';Technique='syntax/schema testing';Profile='PlatformMatrix';Expected='wpm.output.v1 is deterministic UTF-8 JSON with clean stream separation.'},
    @{Requirement='REQ-0018.009';Technique='data-flow/security';Profile='Quality';Expected='Human and JSON selection agree while secrets remain redacted.'}
)
$rationales=@{
    Fast='Controlled installed, cache, repository, trust, audit, and recovery fixtures cover query semantics.'
    PlatformMatrix='Native architectures cover UTF-8 serialization, redirection, path identity, and record readers.'
    Quality='Large and hostile result sets probe ordering, bounds, injection, filters, and redaction.'
    ManualRealEnvironment='Accessibility and support usability receive a controlled human review; runtime behavior remains automated.'
    ReleaseGate='Schema fixtures, query parity, no-mutation snapshots, native matrix, and documentation must pass.'
}
$plan=New-Wpm20PlannedTestPlan 'TC-0018' 'REQ-0018' 'Discoverability and diagnostics' $cases $rationales @('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')
if($Describe){Write-Wpm20PlannedTestPlan $plan;exit 0}
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
