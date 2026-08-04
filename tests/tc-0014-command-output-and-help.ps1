param([switch]$Describe,
      [ValidateSet('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')]
      [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases = @(
    @{ Requirement = 'REQ-0014.001'; Technique = 'use-case'; Profile = 'Fast'; Expected = 'Every public help path has a valid task example.' },
    @{ Requirement = 'REQ-0014.002'; Technique = 'classification-tree'; Profile = 'Fast'; Expected = 'Every semantic class and final result is distinct.' },
    @{ Requirement = 'REQ-0014.003'; Technique = 'decision-table'; Profile = 'PlatformMatrix'; Expected = 'Color policy follows option and TTY state without color-only meaning.' },
    @{ Requirement = 'REQ-0014.004'; Technique = 'state-transition'; Profile = 'Fast'; Expected = 'Lifecycle progress is concise, bounded, and line-oriented when redirected.' },
    @{ Requirement = 'REQ-0014.005'; Technique = 'error-guessing/security'; Profile = 'Quality'; Expected = 'Verbose detail is additive and secret-safe.' },
    @{ Requirement = 'REQ-0014.006'; Technique = 'decision-table'; Profile = 'PlatformMatrix'; Expected = 'Logging preserves structure, redaction, and no-mutation suppression.' },
    @{ Requirement = 'REQ-0014.007'; Technique = 'equivalence-partitioning'; Profile = 'Fast'; Expected = 'Invalid inputs fail with narrow usage and useful help.' },
    @{ Requirement = 'REQ-0014.008'; Technique = 'state-transition'; Profile = 'Fast'; Expected = 'Script streams remain bounded and cannot impersonate WPM results.' }
)
$rationales = @{
    Fast='Deterministic fixtures cover help, semantics, progress, invalid input, and script boundaries.'
    PlatformMatrix='Native x86, x64, and ARM64 cover console, redirection, encoding, and logging behavior.'
    Quality='Bounded hostile text and large-output cases exercise injection, truncation, and redaction.'
    ManualRealEnvironment='A genuine Windows console demonstrates interactive styling and input behavior.'
    ReleaseGate='Fast and native matrix results plus user-output inspection must pass before release.'
}
$plan = New-Wpm20PlannedTestPlan 'TC-0014' 'REQ-0014' 'Command output and help' $cases $rationales @('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')
if ($Describe) { Write-Wpm20PlannedTestPlan $plan; exit 0 }
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
