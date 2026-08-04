$ErrorActionPreference = 'Stop'

function New-Wpm20PlannedTestPlan {
    param(
        [Parameter(Mandatory = $true)][string]$TestCaseId,
        [Parameter(Mandatory = $true)][string]$RequirementId,
        [Parameter(Mandatory = $true)][string]$Title,
        [Parameter(Mandatory = $true)][object[]]$Cases,
        [Parameter(Mandatory = $true)][hashtable]$ProfileRationales,
        [string[]]$RequiredProfiles = @('Fast', 'PlatformMatrix', 'ReleaseGate')
    )

    $profileDefinitions = [ordered]@{
        Fast = 'pull-request'
        PlatformMatrix = '2.0-platform-matrix'
        Quality = 'quality-program'
        ManualRealEnvironment = 'environmental-evidence'
        ReleaseGate = '2.0-release-readiness'
    }
    $profiles = [ordered]@{}
    foreach ($profileName in $profileDefinitions.Keys) {
        if (-not $ProfileRationales.ContainsKey($profileName)) {
            throw "$TestCaseId has no $profileName profile rationale."
        }
        $profiles[$profileName] = [ordered]@{
            Required = $profileName -in $RequiredProfiles
            Rationale = $ProfileRationales[$profileName]
            EvidencePath = "Testing/Evidence/2.0/<source-revision>/$TestCaseId/$profileName/<architecture>/"
            Gate = $profileDefinitions[$profileName]
        }
    }

    [ordered]@{
        Schema = 'wpm.test-plan.v1'
        TestCaseId = $TestCaseId
        RequirementId = $RequirementId
        Title = $Title
        ExecutionState = 'Planned'
        Profiles = $profiles
        Cases = $Cases
    }
}

function Write-Wpm20PlannedTestPlan {
    param([Parameter(Mandatory = $true)]$Plan)
    $Plan | ConvertTo-Json -Depth 8
}

function Stop-Wpm20PlannedExecution {
    param(
        [Parameter(Mandatory = $true)]$Plan,
        [Parameter(Mandatory = $true)]
        [ValidateSet('Fast', 'PlatformMatrix', 'Quality', 'ManualRealEnvironment', 'ReleaseGate')]
        [string]$ExecutionProfile
    )

    [ordered]@{
        Schema = 'wpm.test-execution.v1'
        TestCaseId = $Plan.TestCaseId
        Profile = $ExecutionProfile
        Status = 'Blocked'
        Rationale = 'The controlled test allocation exists, but its product implementation slice and executable assertions are not yet accepted. No verification evidence was produced.'
    } | ConvertTo-Json -Compress
    exit 2
}
