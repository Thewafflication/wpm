[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Lint', 'Build', 'Test')]
    [string]$Stage
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$preset = 'x64-debug'
$build = 'out/build/commit-x64-debug'

function Invoke-CheckedCommand {
    param([string]$Command, [string[]]$Arguments)
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE."
    }
}

Push-Location $root
try {
    switch ($Stage) {
        Lint {
            foreach ($script in @(
                'tools/Test-Style.ps1',
                    'tests/lint-c99.ps1',
                    'tests/verify-traceability.ps1',
                    'tests/verify-traceability-validator.ps1',
                    'tests/verify-wsp-adoption.ps1'
                )) {
                Invoke-CheckedCommand pwsh @('-NoProfile', '-File', $script)
            }
        }
        Build {
            $arguments = @('--preset', $preset, '-B', $build,
                '-DBUILD_TESTING=ON', '-DWPM_BUILD_TEST_REPORTS=OFF',
                '-DWPM_RUN_TESTS_AFTER_BUILD=OFF')
            if ($env:WPM_NINJA) {
                $arguments += "-DCMAKE_MAKE_PROGRAM=$env:WPM_NINJA"
            }
            Invoke-CheckedCommand cmake $arguments
            Invoke-CheckedCommand cmake @('--build', $build, '--parallel')
        }
        Test {
            Invoke-CheckedCommand ctest @('--test-dir', $build,
                '-C', 'Debug', '--output-on-failure', '--no-tests=error')
        }
    }
} catch {
    Write-Error "WPM $Stage commit check failed: $_"
    exit 1
} finally {
    Pop-Location
}
