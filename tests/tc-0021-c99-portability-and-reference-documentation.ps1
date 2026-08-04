param([switch]$Describe,
      [ValidateSet('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')]
      [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases=@(
    @{Requirement='REQ-0021.001';Technique='static analysis/negative compilation';Profile='Fast';Expected='Portable core compiles as strict C99 and rejects controlled C11 fixtures.'},
    @{Requirement='REQ-0021.002';Technique='structure-based testing';Profile='Fast';Expected='Portable modules contain no undeclared Windows dependency.'},
    @{Requirement='REQ-0021.003';Technique='fault seeding';Profile='Fast';Expected='CI rejects C11 constructs, platform leakage, missing sources, and warnings.'},
    @{Requirement='REQ-0021.004';Technique='checklist/schema testing';Profile='Fast';Expected='Every public entity has all required Doxygen contract fields.'},
    @{Requirement='REQ-0021.005';Technique='checklist inspection';Profile='ManualRealEnvironment';Expected='Module and algorithm contracts document security invariants and bounds.'},
    @{Requirement='REQ-0021.006';Technique='negative documentation fixture';Profile='ReleaseGate';Expected='Warning-as-error reference generation publishes a complete artifact.'},
    @{Requirement='REQ-0021.007';Technique='configuration/regression testing';Profile='PlatformMatrix';Expected='Refactoring preserves behavior, formats, trust, containment, bounds, and redaction.'}
)
$rationales=@{
    Fast='Strict compile, source-boundary, documentation-coverage, and seeded-negative checks run on each change.'
    PlatformMatrix='Native x86, x64, and ARM64 builds and regressions cover platform helpers and supported Windows behavior.'
    Quality='Static-analysis depth and resource-bound stress supplement fast compilation and regressions.'
    ManualRealEnvironment='Security/interface documentation receives maintainer inspection where semantic adequacy cannot be automated.'
    ReleaseGate='Strict C99, warnings, reference generation, documentation coverage, and native regressions must pass.'
}
$plan=New-Wpm20PlannedTestPlan 'TC-0021' 'REQ-0021' 'C99 portability and reference documentation' $cases $rationales @('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')
if($Describe){Write-Wpm20PlannedTestPlan $plan;exit 0}
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
