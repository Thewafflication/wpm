param([switch]$Describe,
    [ValidateSet('Fast', 'PlatformMatrix', 'Quality', 'ManualRealEnvironment', `
            'ReleaseGate')]
    [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases = @(
    @{Requirement = 'REQ-0020.001'; Technique = 'structure-based/self-test'; `
            Profile = 'Fast'; Expected = (
            'Harness isolation rejects production roots, ' +
            'keys, trust, packages, and data.'
        )
    },
    @{Requirement = 'REQ-0020.002'; Technique = 'schema testing'; Profile = `
        'Fast'; `
            Expected = (
            'Execution records contain exact reproducible' +
            ', secret-safe metadata.'
        )
    },
    @{Requirement = 'REQ-0020.003'; Technique = 'classification-tree'; `
            Profile = 'Fast'; Expected = (
            'Every corpus class has controlled purpose, o' +
            'rigin, result, requirement, and threat metad' +
            'ata.'
        )
    },
    @{Requirement = 'REQ-0020.004'; Technique = `
        'boundary-value/fault injection'; `
            Profile = 'Quality'; Expected = (
            'Time, iteration, disk, memory, process, and ' +
            'artifact limits end controllably.'
        )
    },
    @{Requirement = 'REQ-0020.005'; Technique = 'state-transition'; `
            Profile = 'Quality'; Expected = (
            'Failure, triage, minimization, promotion, co' +
            'rrection, and rerun remain linked.'
        )
    },
    @{Requirement = 'REQ-0020.006'; Technique = 'decision-table'; `
            Profile = 'ReleaseGate'; Expected = (
            'Any required non-Pass or unreviewed finding ' +
            'blocks the candidate.'
        )
    },
    @{Requirement = 'REQ-0020.007'; Technique = 'configuration testing'; `
            Profile = 'PlatformMatrix'; Expected = (
            'Architecture and environmental evidence accu' +
            'rately distinguishes native, emulated, build' +
            '-only, and manual.'
        )
    }
)
$rationales = @{
    Fast = (
        'Harness, schema, corpus, and synthetic gate ' +
        'self-tests run deterministically without lon' +
        'g campaigns.'
    )
    PlatformMatrix = (
        'Metadata and smoke campaigns prove architect' +
        'ure classification on x86, x64, and ARM64.'
    )
    Quality = (
        'Nightly and prerelease bounded campaigns exe' +
        'rcise endurance, fuzzing, faults, media, tra' +
        'nsports, and recovery.'
    )
    ManualRealEnvironment = (
        'Environmental transport/media cases retain o' +
        'perator, environment, limitation, and approv' +
        'al evidence.'
    )
    ReleaseGate = (
        'A controlled completion record with all requ' +
        'ired Pass statuses and triaged findings is m' +
        'andatory.'
    )
}
$plan = New-Wpm20PlannedTestPlan 'TC-0020' 'REQ-0020' `
    'Quality testing and resilience' $cases $rationales @('Fast', `
        'PlatformMatrix', 'Quality', 'ManualRealEnvironment', 'ReleaseGate')
if ($Describe) {
    Write-Wpm20PlannedTestPlan $plan; exit 0
}
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
