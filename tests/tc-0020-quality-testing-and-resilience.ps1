param([switch]$Describe,
      [ValidateSet('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')]
      [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases=@(
    @{Requirement='REQ-0020.001';Technique='structure-based/self-test';Profile='Fast';Expected='Harness isolation rejects production roots, keys, trust, packages, and data.'},
    @{Requirement='REQ-0020.002';Technique='schema testing';Profile='Fast';Expected='Execution records contain exact reproducible, secret-safe metadata.'},
    @{Requirement='REQ-0020.003';Technique='classification-tree';Profile='Fast';Expected='Every corpus class has controlled purpose, origin, result, requirement, and threat metadata.'},
    @{Requirement='REQ-0020.004';Technique='boundary-value/fault injection';Profile='Quality';Expected='Time, iteration, disk, memory, process, and artifact limits end controllably.'},
    @{Requirement='REQ-0020.005';Technique='state-transition';Profile='Quality';Expected='Failure, triage, minimization, promotion, correction, and rerun remain linked.'},
    @{Requirement='REQ-0020.006';Technique='decision-table';Profile='ReleaseGate';Expected='Any required non-Pass or unreviewed finding blocks the candidate.'},
    @{Requirement='REQ-0020.007';Technique='configuration testing';Profile='PlatformMatrix';Expected='Architecture and environmental evidence accurately distinguishes native, emulated, build-only, and manual.'}
)
$rationales=@{
    Fast='Harness, schema, corpus, and synthetic gate self-tests run deterministically without long campaigns.'
    PlatformMatrix='Metadata and smoke campaigns prove architecture classification on x86, x64, and ARM64.'
    Quality='Nightly and prerelease bounded campaigns exercise endurance, fuzzing, faults, media, transports, and recovery.'
    ManualRealEnvironment='Environmental transport/media cases retain operator, environment, limitation, and approval evidence.'
    ReleaseGate='A controlled completion record with all required Pass statuses and triaged findings is mandatory.'
}
$plan=New-Wpm20PlannedTestPlan 'TC-0020' 'REQ-0020' 'Quality testing and resilience' $cases $rationales @('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')
if($Describe){Write-Wpm20PlannedTestPlan $plan;exit 0}
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
