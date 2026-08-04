param([switch]$Describe,
      [ValidateSet('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')]
      [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases=@(
    @{Requirement='REQ-0019.001';Technique='state-transition/fault injection';Profile='Quality';Expected='Every failure phase retains a complete secret-safe recovery record and safe next action.'},
    @{Requirement='REQ-0019.002';Technique='syntax/negative testing';Profile='Fast';Expected='Inspection is read-only and rejects missing, changed, malformed, or untrusted inputs.'},
    @{Requirement='REQ-0019.003';Technique='state-transition';Profile='Fast';Expected='Retry recomputes and reauthorizes current trusted state without blind replay.'},
    @{Requirement='REQ-0019.004';Technique='classification-tree';Profile='Fast';Expected='Cleanup classification, plan, dry run, confirmation, and idempotence agree.'},
    @{Requirement='REQ-0019.005';Technique='fault injection/security';Profile='Quality';Expected='Unsafe roots, links, locks, races, and evidence retention fail closed.'},
    @{Requirement='REQ-0019.006';Technique='checklist inspection';Profile='ManualRealEnvironment';Expected='Uninstall guidance distinguishes every retained and removable state class.'},
    @{Requirement='REQ-0019.007';Technique='scenario testing';Profile='PlatformMatrix';Expected='Whole-state backup/restore is validated and partial-restore risks remain explicit.'},
    @{Requirement='REQ-0019.008';Technique='configuration testing';Profile='PlatformMatrix';Expected='Native architecture results preserve linked original failures and reruns.'},
    @{Requirement='REQ-0019.009';Technique='decision-table';Profile='Fast';Expected='External script uncertainty never becomes a universal rollback claim.'}
)
$rationales=@{
    Fast='Disposable records and roots cover inspection, retry, cleanup, idempotence, and honest output.'
    PlatformMatrix='Native x86, x64, and ARM64 cover filesystem, process, restore, and self-upgrade recovery.'
    Quality='Lifecycle fault injection, tampering, races, links, locks, and repeated recovery preserve evidence.'
    ManualRealEnvironment='Administrator backup/restore and destructive-boundary review validate real permissions and guidance.'
    ReleaseGate='All lifecycle phases, native architectures, retained failures, and destructive-boundary reviews must pass.'
}
$plan=New-Wpm20PlannedTestPlan 'TC-0019' 'REQ-0019' 'Recovery and lifecycle cleanup' $cases $rationales @('Fast','PlatformMatrix','Quality','ManualRealEnvironment','ReleaseGate')
if($Describe){Write-Wpm20PlannedTestPlan $plan;exit 0}
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
