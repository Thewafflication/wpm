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
$testReportWorkflow = Join-Path $PSScriptRoot '..\.github\workflows\test-reports.yml'
$results = @(
    New-WpmManualStep `
        -Name 'Validate tag-triggered Release workflow configuration' `
        -Action {
            $workflow = Get-Content -Raw -LiteralPath $releaseWorkflow
            $requiredPatterns = @(
                'tags:',
                'dependency-metadata:',
                'git submodule foreach --recursive ''git fetch --tags --force''',
                'export-dependency-metadata\.ps1',
                'WPM_MINIZ_VERSION_OVERRIDE=\$\{\{ needs\.dependency-metadata\.outputs\.miniz_version \}\}',
                'WPM_LIBSODIUM_VERSION_OVERRIDE=\$\{\{ needs\.dependency-metadata\.outputs\.libsodium_version \}\}',
                'x86-release',
                'x64-release',
                'arm64-release',
                'name: wpm-release-verification-x86',
                'path: bin/x86/Debug',
                'WPM_EXECUTABLE=.*bin/x86/Debug/wpm\.exe',
                'WPM_PACKAGE_EXECUTABLE=.*release-binaries/wpm-\$architecture\.exe',
                'environment: release',
                'secrets\.WPM_RELEASE_PRIVATE_KEY',
                'WPM_PACKAGE_SIGNING_KEY=\$env:WPM_RELEASE_KEY_PATH',
                'trust add release_keys/wpm-release\.public',
                'wpm\.exe verify \$package\.FullName',
                '\$packages = Get-ChildItem -LiteralPath release/packages -Filter ''wpm-\*\.zip''',
                'release/packages/wpm-\*\.zip',
                'release/keys/wpm-release\.public',
                'release/install\.cmd'
            )

            foreach ($pattern in $requiredPatterns) {
                if ($workflow -notmatch $pattern) {
                    throw "Release workflow is missing required configuration: $pattern"
                }
            }
            if ($workflow -match '(?m)^\s*- name: Generate ephemeral release signing key') {
                throw 'Release workflow still generates an ephemeral release signing key.'
            }
            if ([regex]::Matches($workflow, 'git submodule foreach --recursive ''git fetch --tags --force''').Count -ne 1) {
                throw 'Release workflow must fetch dependency tags exactly once in its metadata job.'
            }
            if ([regex]::Matches($workflow, 'cmake --preset x86-debug-reports').Count -ne 0 -or
                [regex]::Matches($workflow, 'cmake --build --preset verify-x86-debug').Count -ne 0) {
                throw 'Release workflow redundantly rebuilds the verified x86 Debug package builder.'
            }

            if ($env:GITHUB_REF_TYPE -ne 'tag') {
                'Not a tag run; release publication is not required.'
            }
            else {
                'Tag run detected; the Release workflow will verify, package, and publish the architecture-specific assets.'
            }
        }

    New-WpmManualStep `
        -Name 'Validate development-build Git history configuration' `
        -Action {
            $workflow = Get-Content -Raw -LiteralPath $testReportWorkflow
            if ($workflow -notmatch 'fetch-depth:\s*0') {
                throw 'Test-report workflow uses a shallow checkout and cannot derive versions from the last release tag.'
            }
            if ([regex]::Matches($workflow, 'git submodule foreach --recursive ''git fetch --tags --force''').Count -ne 1 -or
                $workflow -notmatch 'export-dependency-metadata\.ps1' -or
                $workflow -notmatch 'needs:\s*dependency-metadata') {
                throw 'Test-report workflow does not resolve dependency metadata once for all architecture jobs.'
            }
            'Test-report builds check out complete Git history for version generation.'
        }
)
$finished = Get-Date

if ($EvidenceTex) {
    Write-WpmTestEvidence -TestCaseId 'TC-0009' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
