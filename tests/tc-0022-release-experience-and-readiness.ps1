param([switch]$Describe,
    [ValidateSet('Fast', 'PlatformMatrix', 'Quality', 'ManualRealEnvironment', `
            'ReleaseGate')]
    [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases = @(
    @{Requirement = 'REQ-0022.001'; Technique = 'checklist inspection'; `
            Profile = 'Fast'; Expected = (
            'Migration guidance covers preserved contract' +
            's, changes, recovery, compatibility, and lim' +
            'its.'
        )
    },
    @{Requirement = 'REQ-0022.002'; Technique = 'use-case/example testing'; `
            Profile = 'Fast'; Expected = (
            'Release claims and commands agree with execu' +
            'table evidence.'
        )
    },
    @{Requirement = 'REQ-0022.003'; Technique = 'checklist review'; `
            Profile = 'ManualRealEnvironment'; Expected = (
            'Support and user artifacts state all platfor' +
            'm, transport, HTTP, script, rollback, and re' +
            'covery limits.'
        )
    },
    @{Requirement = 'REQ-0022.004'; Technique = 'scenario/state-transition'; `
            Profile = 'PlatformMatrix'; Expected = (
            'Bootstrap and prior-stable self-upgrade jour' +
            'neys cover success and failed handoff.'
        )
    },
    @{Requirement = 'REQ-0022.005'; Technique = 'configuration testing'; `
            Profile = 'PlatformMatrix'; Expected = (
            'Every journey has native or approved-emulato' +
            'r evidence on each architecture.'
        )
    },
    @{Requirement = 'REQ-0022.006'; `
            Technique = 'checklist/cryptographic inspection'; Profile = `
                'ReleaseGate'; `
            Expected = (
            'Command safety, recovery, quality, artifacts' +
            ', signing, scanning, provenance, and digests' +
            ' pass.'
        )
    },
    @{Requirement = 'REQ-0022.007'; Technique = `
        'decision-table/record review'; `
            Profile = 'ReleaseGate'; Expected = (
            'Release Readiness identifies the exact basel' +
            'ine and blocks every required non-Pass.'
        )
    },
    @{Requirement = 'REQ-0022.008'; Technique = 'record-schema review'; `
            Profile = 'ReleaseGate'; Expected = (
            'Publication record binds approval, identitie' +
            's, evidence, exceptions, limitations, and su' +
            'pport.'
        )
    },
    @{Requirement = 'REQ-0022.009'; Technique = 'process review'; `
            Profile = 'ManualRealEnvironment'; Expected = (
            'Retrospective improvements have owners, outc' +
            'omes, approvals, conditions, and measures.'
        )
    }
)
$rationales = @{
    Fast = (
        'Static document, example, record-schema, and' +
        ' synthetic non-Pass gate checks run before r' +
        'elease workflows.'
    )
    PlatformMatrix = (
        'Native bootstrap, prior-stable upgrade, reco' +
        'very, and documented journeys run on every a' +
        'rchitecture.'
    )
    Quality = (
        'The controlled quality completion record and' +
        ' retained failure chains feed readiness.'
    )
    ManualRealEnvironment = (
        'Maintainer, security, support, and release-a' +
        'pprover reviews plus retrospective require a' +
        'ccountable humans.'
    )
    ReleaseGate = (
        'Exact artifact, trust, evidence, readiness, ' +
        'publication, and support records block publi' +
        'cation when incomplete.'
    )
}
$plan = New-Wpm20PlannedTestPlan 'TC-0022' 'REQ-0022' `
    'Release experience and readiness' $cases $rationales @('Fast', `
        'PlatformMatrix', 'Quality', 'ManualRealEnvironment', 'ReleaseGate')
if ($Describe) {
    Write-Wpm20PlannedTestPlan $plan; exit 0
}
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
