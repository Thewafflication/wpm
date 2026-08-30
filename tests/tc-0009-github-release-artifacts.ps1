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
$toolchainAction = Join-Path $PSScriptRoot '..\.github\actions\setup-tinycc\action.yml'
$tinyccToolchain = Join-Path $PSScriptRoot '..\cmake\toolchains\tcc-windows.cmake'
$cmakePresets = Join-Path $PSScriptRoot '..\CMakePresets.json'
$rootCmake = Join-Path $PSScriptRoot '..\CMakeLists.txt'
$thirdPartyCmake = Join-Path $PSScriptRoot '..\third_party\CMakeLists.txt'
$wpmCmake = Join-Path $PSScriptRoot '..\wpm\CMakeLists.txt'
$previousReleaseUpgradeTest = Join-Path $PSScriptRoot 'verify-previous-release-upgrade.ps1'
$xpWorkflow = Join-Path $PSScriptRoot '..\.github\workflows\xp-release.yml'
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
                'WPM_MINIZIP_NG_VERSION_OVERRIDE=\$\{\{ needs\.dependency-metadata\.outputs\.minizip_ng_version \}\}',
                'WPM_ZLIB_NG_VERSION_OVERRIDE=\$\{\{ needs\.dependency-metadata\.outputs\.zlib_ng_version \}\}',
                'WPM_LIBSODIUM_VERSION_OVERRIDE=\$\{\{ needs\.dependency-metadata\.outputs\.libsodium_version \}\}',
                'x86-release',
                'x64-release',
                'arm64-release',
                'architecture: \$\{\{ matrix\.arch \}\}',
                'name: wpm-release-verification-x86',
                'path: bin/x86/Debug',
                'WPM_EXECUTABLE=.*bin/x86/Debug/wpm\.exe',
                'WPM_PACKAGE_EXECUTABLE=.*release-binaries/wpm-\$architecture\.exe',
                'WPM_PACKAGE_RUNTIME_DLL=.*release-binaries/wcrt-\$architecture\.dll',
                'release/wcrt-\$\{\{ matrix\.arch \}\}\.dll',
                'environment: release',
                'secrets\.WPM_RELEASE_PRIVATE_KEY',
                'WPM_PACKAGE_SIGNING_KEY=\$env:WPM_RELEASE_KEY_PATH',
                'trust add release_keys/wpm-release\.public',
                'wpm\.exe verify \$package\.FullName',
                'name: Verify self-contained Release executable',
                'runs-on: \$\{\{ matrix\.runner \}\}',
                'runner: windows-11-arm',
                'name: Upgrade \$\{\{ matrix\.arch \}\} from previous release',
                'GH_TOKEN: \$\{\{ github\.token \}\}',
                'gh api "repos/\$\{\{ github\.repository \}\}/releases\?per_page=100"',
                '-not \$_\.draft -and -not \$_\.prerelease',
                '@\(\$_\.assets\.name\) -contains \$expectedAsset',
                'git merge-base --is-ancestor \$release\.Tag HEAD\^',
                'verify-previous-release-upgrade\.ps1',
                'needs:\s*\[[^\]]*upgrade-compatibility[^\]]*\]',
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
            if ($workflow -match 'git tag --merged HEAD\^ --sort=-version:refname') {
                throw 'Previous-release upgrade selection must use published release assets, not bare Git tags.'
            }
            $upgradeTest = Get-Content -Raw -LiteralPath $previousReleaseUpgradeTest
            foreach ($pattern in @(
                'releases/download/\$PreviousVersion',
                'upgrade wpm --arch \$Architecture --version \$CandidateVersion --offline',
                'Result: wpm \$Architecture upgraded',
                '\$installedExecutable --version'
            )) {
                if ($upgradeTest -notmatch $pattern) {
                    throw "Previous-release upgrade gate is missing required behavior: $pattern"
                }
            }
            if ($workflow -match 'tcc-x86-xp|WPM_WINDOWS_XP_COMPAT') {
                throw 'Release workflow must use WCRT rather than the custom XP runtime.'
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
                $workflow -notmatch 'needs:\s*dependency-metadata' -or
                $workflow -notmatch 'architecture: \$\{\{ matrix\.arch \}\}') {
                throw 'Test-report workflow does not resolve dependency metadata once for all architecture jobs.'
            }
            foreach ($pattern in @(
                'name: Build unsigned Debug WPM package',
                "if: steps\.verify\.outputs\.exit_code == '0'",
                'WPM_DATA_DIR.+wpm-debug-package-\$\{\{ matrix\.arch \}\}',
                'WPM_PACKAGE_EXECUTABLE=.*bin/\$\{\{ matrix\.arch \}\}/Debug/wpm\.exe',
                'WPM_PACKAGE_DEBUG=true',
                'name: wpm-windows-\$\{\{ matrix\.arch \}\}-debug',
                'debug-packages/wpm-\$\{\{ matrix\.arch \}\}-debug-\*\.zip',
                'name: Upload failed test diagnostics',
                "if: steps\.verify\.outputs\.exit_code != '0'"
            )) {
                if ($workflow -notmatch $pattern) {
                    throw "Test-report workflow is missing conditional Debug packaging configuration: $pattern"
                }
            }
            'Test-report builds check out complete Git history for version generation.'
        }

    New-WpmManualStep `
        -Name 'Require TinyCC and multi-architecture WCRT in Windows CI' `
        -Action {
            $action = Get-Content -Raw -LiteralPath $toolchainAction
            foreach ($pattern in @(
                'architecture:',
                'Thewafflication/wcrt/releases/latest/download',
                'Get-FileHash.+\$tinyccPublicKey',
                'Get-FileHash.+\$wcrtPublicKey',
                '\$wcrtKeyHash -eq \$tinyccKeyHash',
                'trust add \$wcrtPublicKey',
                'install wcrt --arch any',
                'WCRT architecture mismatch',
                'WPM_WCRT_ROOT='
            )) {
                if ($action -notmatch $pattern) { throw "Toolchain setup is missing WCRT configuration: $pattern" }
            }

            $presets = (Get-Content -Raw -LiteralPath $cmakePresets | ConvertFrom-Json).configurePresets
            $windowsBase = $presets | Where-Object name -eq 'windows-base'
            if (-not $windowsBase -or $windowsBase.toolchainFile -notmatch 'tcc-windows\.cmake') {
                throw 'Windows presets must use the TinyCC-only Windows toolchain.'
            }
            foreach ($name in @('x86-debug', 'x64-debug', 'arm64-debug')) {
                $preset = $presets | Where-Object name -eq $name
                if (-not $preset -or
                    $preset.cacheVariables.PSObject.Properties.Name -contains 'WPM_USE_WCRT' -or
                    $preset.cacheVariables.PSObject.Properties.Name -contains 'WPM_WINDOWS_XP_COMPAT') {
                    throw "$name must use the unconditional TinyCC/WCRT Windows configuration."
                }
            }
            $rootConfiguration = Get-Content -Raw -LiteralPath $rootCmake
            if ($rootConfiguration -notmatch 'WPM Windows builds require TinyCC' -or
                $rootConfiguration -notmatch 'WCRT 1\.1\.1 or newer' -or
                $rootConfiguration -match 'CMAKE_MSVC|if\s*\(MSVC') {
                throw 'Root CMake configuration must require TinyCC and WCRT 1.1.1 without MSVC branches.'
            }
            $thirdPartyConfiguration = Get-Content -Raw -LiteralPath $thirdPartyCmake
            if ($thirdPartyConfiguration -match 'builds[/\\]msvc|\bMSVC\b|_MSC_VER') {
                throw 'Third-party configuration must not depend on MSVC build metadata or compiler branches.'
            }
            if ($thirdPartyConfiguration -notmatch 'WCRT_POSIX=1' -or
                $thirdPartyConfiguration -match 'EFBIG=27|ENXIO=6') {
                throw 'Third-party builds must use the WCRT bounded POSIX compatibility profile.'
            }
            $toolchain = Get-Content -Raw -LiteralPath $tinyccToolchain
            if ($toolchain -notmatch 'TinyCCArchiver\.ps1' -or
                $toolchain -notmatch 'WPM_TCC_COMPILER_BASE64' -or
                $toolchain -notmatch 'ToBase64String' -or
                $toolchain -match 'tcc-ar\.cmd') {
                throw 'TinyCC archive rules must preserve compiler paths containing spaces.'
            }
            $cmake = Get-Content -Raw -LiteralPath $wpmCmake
            if ($cmake -notmatch 'WPM_WCRT_TARGET_ROOT}/lib/libwcrt\.a' -or
                $cmake -notmatch 'WPM_WCRT_TARGET_ROOT}/lib/wcrt-startup-console\.o' -or
                $cmake -match 'WPM_WCRT_TARGET_ROOT}/lib/wcrt\.def') {
                throw 'WPM must link the static WCRT library and console startup object.'
            }
            if (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $wpmCmake) 'tcc_support\wcrt_start.c')) {
                throw 'WPM must use the WCRT-provided console startup object.'
            }
            if ($cmake -match 'tcc_support/(?:wcrt_stat|utime)\.c') {
                throw 'WPM must use WCRT-provided file status and time implementations.'
            }
            if (Test-Path -LiteralPath $xpWorkflow) { throw 'The custom XP runtime workflow must not be restored.' }
            'Every supported Windows architecture is configured from the multi-architecture WCRT package.'
        }
)
$finished = Get-Date

if ($EvidenceTex) {
    Write-WpmTestEvidence -TestCaseId 'TC-0009' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
