param([switch]$Describe,
      [ValidateSet('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')]
      [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases = @(
    @{ Requirement='REQ-0015.001'; Technique='use-case'; Profile='Fast'; Expected='Plans completely identify consequential mutations before confirmation.' },
    @{ Requirement='REQ-0015.002'; Technique='state-transition'; Profile='Fast'; Expected='Planned, active, partial, deferred, and final states are never conflated.' },
    @{ Requirement='REQ-0015.003'; Technique='decision-table'; Profile='PlatformMatrix'; Expected='Affirmative, rejection, empty, EOF, -y, and --yes select the documented branch.' },
    @{ Requirement='REQ-0015.004'; Technique='state-transition/state-difference'; Profile='Fast'; Expected='Dry run produces a plan with zero durable mutation or script execution.' },
    @{ Requirement='REQ-0015.005'; Technique='equivalence-partitioning'; Profile='Fast'; Expected='Unsupported or contradictory option combinations fail closed.' },
    @{ Requirement='REQ-0015.006'; Technique='cause-effect-graphing'; Profile='Quality'; Expected='Identity, source, trust, or state change invalidates prior authorization.' }
)
$rationales=@{
    Fast='Disposable fixtures compare complete before/after state and deterministic plans.'
    PlatformMatrix='Native architectures cover CRT input, console input, filesystem, and handoff differences.'
    Quality='Fault injection changes plan inputs and probes mutation-capability boundaries.'
    ManualRealEnvironment='A genuine Windows console proves affirmative input is consumed rather than EOF.'
    ReleaseGate='Every supported mutating command must pass plan, confirmation, and dry-run coverage.'
}
$plan=New-Wpm20PlannedTestPlan 'TC-0015' 'REQ-0015' 'Safer package changes' $cases $rationales @('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')
if($Describe){Write-Wpm20PlannedTestPlan $plan;exit 0}
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
