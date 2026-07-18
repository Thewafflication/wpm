param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,

    [string]$EvidenceTex,

    [switch]$NoFailOnFailure
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$started = Get-Date
$releaseWorkflow = Join-Path $PSScriptRoot '..\.github\workflows\release.yml'
$results = @(
    New-WpmManualStep `
        -Name 'Validate tag-triggered Release workflow configuration' `
        -Action {
            $workflow = Get-Content -Raw -LiteralPath $releaseWorkflow
            $requiredPatterns = @(
                'tags:',
                'git submodule foreach --recursive ''git fetch --tags --force''',
                'x86-release',
                'x64-release',
                'arm64-release',
                'WPM_EXECUTABLE=.*bin/x86/Debug/wpm\.exe',
                'WPM_PACKAGE_EXECUTABLE=.*release-binaries/wpm-\$architecture\.exe',
                'environment: release',
                'secrets\.WPM_RELEASE_PRIVATE_KEY',
                'WPM_PACKAGE_SIGNING_KEY=\$env:WPM_RELEASE_KEY_PATH',
                'trust add release_keys/wpm-release\.public',
                'wpm\.exe verify \$package\.FullName',
                '\$packages = Get-ChildItem -LiteralPath release/packages -Filter ''wpm-\*\.zip''',
                'release/packages/wpm-\*\.zip',
                'release/keys/wpm-release\.public'
            )

            foreach ($pattern in $requiredPatterns) {
                if ($workflow -notmatch $pattern) {
                    throw "Release workflow is missing required configuration: $pattern"
                }
            }
            if ($workflow -match '(?m)^\s*- name: Generate ephemeral release signing key') {
                throw 'Release workflow still generates an ephemeral release signing key.'
            }

            if ($env:GITHUB_REF_TYPE -ne 'tag') {
                'Not a tag run; release publication is not required.'
            }
            else {
                'Tag run detected; the Release workflow will verify, package, and publish the architecture-specific assets.'
            }
        }
)
$finished = Get-Date

if ($EvidenceTex) {
    Write-WpmTestEvidence -TestCaseId 'TC-0009' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
