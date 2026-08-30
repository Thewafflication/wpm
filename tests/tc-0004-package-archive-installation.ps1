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
$packageName = "wpm-install-$testId"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-tests-$testId"
$wpmDataDir = Join-Path $testRoot 'wpm-data'
$sourceDir = Join-Path $testRoot $packageName
$outputDir = Join-Path $testRoot 'packages'
$deploymentDir = Join-Path $testRoot 'deployment'
$deploymentFile = Join-Path $deploymentDir 'hello.txt'
$archivePackageName = "$packageName-any-1.2.3"
$archivePath = Join-Path $outputDir "$archivePackageName.zip"
$stagingDir = Join-Path $wpmDataDir "temp\$archivePackageName"
$storedArchivePath = Join-Path $wpmDataDir "packages\$archivePackageName.zip"
$previousWpmDataDir = $env:WPM_DATA_DIR

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
    $env:WPM_DATA_DIR = $wpmDataDir
    New-Item -ItemType Directory -Force -Path $sourceDir, $outputDir, $deploymentDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir '.wpm') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir 'nested') | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\package.txt') -Value @(
        "name=$packageName"
        'version=1.2.3'
        'arch=any'
        'debug=false'
    )
    Set-Content -LiteralPath (Join-Path $sourceDir 'hello.txt') -Value 'hello from wpm'
    Set-Content -LiteralPath (Join-Path $sourceDir 'nested\data.txt') -Value 'nested package data'
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\install.cmd') -Value @(
        '@echo off'
        'echo install-script-standard-output'
        'echo install-script-error-output 1>&2'
        "copy /y `"hello.txt`" `"$deploymentFile`" >nul"
    )

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Build setup archive for installation' `
        -Arguments @('build', $sourceDir, $outputDir) `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
                throw "build did not create $archivePath"
            }
        }

    $results += Invoke-WpmTestStep `
        -WpmExe $WpmExe `
        -Name 'Stage, verify, install, and store archive' `
        -Arguments @('install', $archivePath, '--verbose', '--allow-unsigned') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch '(?i)warning.*unsigned') { throw 'Expected unsigned-package warning.' }
            if ($Output -notmatch 'install-script-standard-output' -or $Output -notmatch 'install-script-error-output') {
                throw 'install.cmd standard output and error were not visible.'
            }
            if ($Output -notmatch '(?s)--- install script output ---.*--- end install script output \(exit code 0\) ---') {
                throw 'install.cmd output was not framed with its completion status.'
            }
            $scriptLogs = @(Get-ChildItem -LiteralPath (Join-Path $wpmDataDir 'logs\scripts') -Filter '*-install.log' -File)
            if ($scriptLogs.Count -ne 1) { throw "Expected one install script log, found $($scriptLogs.Count)." }
            $scriptLog = Get-Content -Raw -LiteralPath $scriptLogs[0].FullName
            if ($Output -notmatch [regex]::Escape($scriptLogs[0].FullName) -or
                $scriptLog -notmatch 'install-script-standard-output' -or
                $scriptLog -notmatch 'install-script-error-output' -or
                $scriptLog -notmatch '(?m)^--- exit-code=0 ---$') {
                throw 'Install script output was not streamed and retained in the reported log.'
            }
            if ($Output -notmatch 'WPM process PID: \d+' -or
                $Output -notmatch 'install script process PID: \d+' -or
                $Output -notmatch 'WPM PID \d+ is waiting for install script PID \d+') {
                throw 'Verbose install output did not identify the package-script process relationship.'
            }
            Assert-FileContent $deploymentFile 'hello from wpm'
            if (-not (Test-Path -LiteralPath $storedArchivePath -PathType Leaf)) {
                throw "install did not store $storedArchivePath"
            }
            if (Test-Path -LiteralPath $stagingDir) {
                throw "install did not remove staging directory $stagingDir"
            }
            $escapedPackageName = [regex]::Escape($packageName)
            if ($Output -notmatch "Extraction progress: $escapedPackageName`: 0% \(0/\d+ bytes\)" -or
                $Output -notmatch "Extracted $escapedPackageName`: \d+ bytes" -or
                $Output -notmatch "Validation progress: $escapedPackageName`: 0% \(0/\d+ bytes\)" -or
                $Output -notmatch "Validated $escapedPackageName`: \d+ bytes") {
                throw "Expected redirected extraction and validation byte progress. $Output"
            }
            if ($Output -notmatch [regex]::Escape("$packageName`: Extracting package...") -or
                $Output -notmatch [regex]::Escape("$packageName`: Validating package...") -or
                $Output -notmatch [regex]::Escape("$packageName`: Installing package...") -or
                $Output -notmatch 'Extracting file:' -or
                $Output -notmatch 'Decompressing with zlib-ng:' -or
                $Output -notmatch 'Verifying file:' -or
                $Output -notmatch 'Running install script:' -or $Output -notmatch 'Storing archive:') {
                throw 'Expected verbose installation progress.'
            }
        }

}
finally {
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0004' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
    if ($null -eq $previousWpmDataDir) {
        Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue
    }
    else {
        $env:WPM_DATA_DIR = $previousWpmDataDir
    }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
