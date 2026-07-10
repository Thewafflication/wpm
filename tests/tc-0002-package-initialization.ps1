param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,

    [string]$EvidenceTex,

    [switch]$NoFailOnFailure
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$testId = [Guid]::NewGuid().ToString('N')
$packageName = "wpm-init-$testId"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-tests-$testId"
$sourceDir = Join-Path $testRoot $packageName

function Assert-FileContent {
    param(
        [string]$Path,
        [string]$Expected
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Expected file does not exist: $Path"
    }

    $actual = (Get-Content -Raw -LiteralPath $Path).Trim()
    if ($actual -ne $Expected) {
        throw "Unexpected content in $Path. Expected '$Expected', got '$actual'."
    }
}

$started = Get-Date
$results = @()

try {
    New-Item -ItemType Directory -Force -Path $sourceDir | Out-Null
    Push-Location $sourceDir
    try {
        $results += Invoke-WpmTestStep `
            -WpmExe $WpmExe `
            -Name 'Reject invalid package name' `
            -Arguments @('init', 'bad name') `
            -Assert {
                param($ExitCode, $Output)
                if ($ExitCode -eq 0) {
                    throw 'Expected invalid package name to fail.'
                }
                if (Test-Path -LiteralPath '.wpm') {
                    throw 'Invalid init unexpectedly created .wpm.'
                }
            }

        $results += Invoke-WpmTestStep `
            -WpmExe $WpmExe `
            -Name 'Initialize package using current directory name' `
            -Arguments @('init') `
            -Assert {
                param($ExitCode, $Output)
                if ($ExitCode -ne 0) {
                    throw "Expected exit code 0, got $ExitCode."
                }
                if ($Output -match 'Updating') {
                    throw 'init unexpectedly ran the update command.'
                }

                $expectedFiles = @(
                    '.wpm\package.txt',
                    '.wpm\index.csv',
                    '.wpm\wpmignore.txt',
                    '.wpm\install.cmd',
                    '.wpm\remove.cmd'
                )
                foreach ($relativePath in $expectedFiles) {
                    if (-not (Test-Path -LiteralPath $relativePath -PathType Leaf)) {
                        throw "init did not create $relativePath"
                    }
                }

                $packageMetadata = Get-Content -Raw -LiteralPath '.wpm\package.txt'
                if ($packageMetadata -notmatch "(?m)^name=$([Regex]::Escape($packageName))`r?$") {
                    throw 'init did not write the current directory package name.'
                }

                Assert-FileContent '.wpm\index.csv' 'filename,size,hash,algorithm'
            }

        Set-Content -LiteralPath '.wpm\package.txt' -Value 'preserve=this'

        $results += Invoke-WpmTestStep `
            -WpmExe $WpmExe `
            -Name 'Preserve existing package metadata when init is rerun' `
            -Arguments @('init', $packageName) `
            -Assert {
                param($ExitCode, $Output)
                if ($ExitCode -ne 0) {
                    throw "Expected exit code 0, got $ExitCode."
                }
                Assert-FileContent '.wpm\package.txt' 'preserve=this'
            }
    }
    finally {
        Pop-Location
    }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0002' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
