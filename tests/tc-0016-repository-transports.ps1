param([switch]$Describe,
    [ValidateSet('Fast', 'PlatformMatrix', 'Quality', 'ManualRealEnvironment', `
            'ReleaseGate')]
    [string]$ExecutionProfile = 'Fast')
. (Join-Path $PSScriptRoot 'wpm-2.0-planned-test-lib.ps1')
$cases = @(
    @{Requirement = 'REQ-0016.001'; Technique = 'interface testing'; `
            Profile = 'Fast'; Expected = (
            'All providers preserve the version-1 logical' +
            ' and trust contract.'
        )
    },
    @{Requirement = 'REQ-0016.002'; Technique = 'equivalence-partitioning'; `
            Profile = 'PlatformMatrix'; Expected = (
            'Absolute fixed, removable, and read-only fil' +
            'esystem sources work without source mutation' +
            '.'
        )
    },
    @{Requirement = 'REQ-0016.003'; `
            Technique = 'state-transition/fault injection'; `
            Profile = 'ManualRealEnvironment'; Expected = (
            'SMB success and failures preserve credential' +
            's and prior cache.'
        )
    },
    @{Requirement = 'REQ-0016.004'; Technique = 'decision-table'; Profile = `
        'Fast'; `
            Expected = (
            'HTTP requires repository-scoped opt-in and w' +
            'arns without authorizing downgrade.'
        )
    },
    @{Requirement = 'REQ-0016.005'; Technique = 'classification-tree'; `
            Profile = 'Fast'; Expected = (
            'Every provider applies identical signature a' +
            'nd trust decisions.'
        )
    },
    @{Requirement = 'REQ-0016.006'; Technique = 'data-flow/security'; `
            Profile = 'Fast'; Expected = (
            'Locator identity is useful and credential-sa' +
            'fe in every destination.'
        )
    },
    @{Requirement = 'REQ-0016.007'; `
            Technique = 'state-transition/fault injection'; Profile = `
                'Quality'; `
            Expected = (
            'Loss, substitution, change, and partial read' +
            's fail closed with safe retry.'
        )
    },
    @{Requirement = 'REQ-0016.008'; Technique = `
        'syntax/equivalence-partitioning'; `
            Profile = 'Fast'; Expected = (
            'Relative roots canonicalize; unsafe, ambiguo' +
            'us, device, and escaping locators are reject' +
            'ed.'
        )
    }
)
$rationales = @{
    Fast = (
        'Mock filesystem and HTTP providers determini' +
        'stically cover parsing, policy, trust, and c' +
        'ache preservation.'
    )
    PlatformMatrix = (
        'Native architectures cover Windows path root' +
        's, read-only attributes, and provider integr' +
        'ation.'
    )
    Quality = (
        'Bounded transport faults, media changes, par' +
        'tial reads, and hostile locators exercise fa' +
        'il-closed behavior.'
    )
    ManualRealEnvironment = (
        'Managed SMB, removable USB, read-only optica' +
        'l media, and real HTTP/TLS policy require co' +
        'ntrolled demonstrations.'
    )
    ReleaseGate = (
        'Fast, native matrix, quality, and managed-en' +
        'vironment transport evidence are all require' +
        'd.'
    )
}
$plan = New-Wpm20PlannedTestPlan 'TC-0016' 'REQ-0016' 'Repository transports' `
    $cases $rationales @('Fast', 'PlatformMatrix', 'Quality', `
        'ManualRealEnvironment', 'ReleaseGate')
if ($Describe) {
    Write-Wpm20PlannedTestPlan $plan; exit 0
}
Stop-Wpm20PlannedExecution $plan $ExecutionProfile
