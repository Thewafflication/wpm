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
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-signing-$testId"
$dataDir = Join-Path $testRoot 'wpm-data'
$outputDir = Join-Path $testRoot 'packages'
$keyDir = Join-Path $testRoot 'keys'
$sourceDir = Join-Path $testRoot 'source'
$untrustedSourceDir = Join-Path $testRoot 'untrusted-source'
$unsignedSourceDir = Join-Path $testRoot 'unsigned-source'
$deployment = Join-Path $testRoot 'deployment.txt'
$privateKey = Join-Path $keyDir 'maintainer.private'
$publicKey = Join-Path $keyDir 'maintainer.public'
$untrustedPrivateKey = Join-Path $keyDir 'untrusted.private'
$untrustedPublicKey = Join-Path $keyDir 'untrusted.public'
$packageName = "signing-test-$testId"
$untrustedPackageName = "untrusted-test-$testId"
$unsignedPackageName = "unsigned-test-$testId"
$archivePath = Join-Path $outputDir "$packageName-any-1.0.0.zip"
$untrustedArchivePath = Join-Path $outputDir "$untrustedPackageName-any-1.0.0.zip"
$unsignedArchivePath = Join-Path $outputDir "$unsignedPackageName-any-1.0.0.zip"
$previousDataDir = $env:WPM_DATA_DIR
$started = Get-Date
$results = @()
$untrustedKeyId = $null
$malformedArchive = $null
$invalidArchive = $null
$unindexedArchive = $null

function New-TestPackage {
    param([string]$Path, [string]$Name, [string]$Marker)

    New-Item -ItemType Directory -Force -Path (Join-Path $Path '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $Path '.wpm\package.txt') -Value @(
        "name=$Name"
        'version=1.0.0'
        'arch=any'
        'debug=false'
    )
    Set-Content -LiteralPath (Join-Path $Path '.wpm\install.cmd') -Value @(
        '@echo off'
        "echo $Marker > `"$deployment`""
    )
    Set-Content -LiteralPath (Join-Path $Path 'payload.txt') -Value $Marker
}

function New-ArchiveVariant {
    param([string]$Archive, [string]$VariantName, [scriptblock]$Mutate)

    $variantDir = Join-Path $testRoot $VariantName
    $variantPath = Join-Path $outputDir "$VariantName.zip"
    Expand-Archive -LiteralPath $Archive -DestinationPath $variantDir -Force
    & $Mutate $variantDir
    Compress-Archive -Path (Join-Path $variantDir '*') -DestinationPath $variantPath -Force
    return $variantPath
}

try {
    $env:WPM_DATA_DIR = $dataDir
    New-Item -ItemType Directory -Force -Path $outputDir, $keyDir, $sourceDir, $untrustedSourceDir, $unsignedSourceDir | Out-Null
    New-TestPackage -Path $sourceDir -Name $packageName -Marker 'signed'
    New-TestPackage -Path $untrustedSourceDir -Name $untrustedPackageName -Marker 'untrusted'
    New-TestPackage -Path $unsignedSourceDir -Name $unsignedPackageName -Marker 'unsigned'

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Generate and designate default signing key' -Arguments @('keygen', $privateKey, $publicKey, '--default') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Expected key generation to succeed. $Output" }
        if (-not (Test-Path -LiteralPath $privateKey) -or -not (Test-Path -LiteralPath $publicKey)) { throw 'keygen did not create both key files.' }
        if ($Output -match 'secret-key=') { throw 'keygen displayed private-key material.' }
        if (-not (Test-Path -LiteralPath (Join-Path $dataDir 'config\signing-key.txt'))) { throw 'keygen did not configure the default signing key.' }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build signed package with default key' -Arguments @('build', $sourceDir, $outputDir) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or -not (Test-Path -LiteralPath $archivePath)) { throw "Default-key build failed. $Output" }
    }

    $results += New-WpmManualStep -Name 'Inspect signed package format' -Action {
        $inspectDir = Join-Path $testRoot 'inspect'
        Expand-Archive -LiteralPath $archivePath -DestinationPath $inspectDir -Force
        $signature = Get-Content -Raw -LiteralPath (Join-Path $inspectDir '.wpm\signature.json') | ConvertFrom-Json
        if ($signature.version -ne 1 -or $signature.algorithm -ne 'ed25519' -or $signature.key_id -notmatch '^[0-9a-f]{64}$' -or -not $signature.signature) { throw 'Unexpected signature metadata.' }
        $index = Get-Content -Raw -LiteralPath (Join-Path $inspectDir '.wpm\index.csv')
        if ($index -match '(?m)^\.wpm/(index\.csv|signature\.json),') { throw 'Index includes a signature control file.' }
        'Signed package has one valid signature metadata file.'
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject package signed by unknown key' -Arguments @('install', $archivePath) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0) { throw 'Unknown-key package was installed.' }
        if (Test-Path -LiteralPath $deployment) { throw 'Unknown-key package ran its install script.' }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Trust signing public key' -Arguments @('trust', 'add', $publicKey) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Could not add trusted key. $Output" }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Install package with trusted valid signature' -Arguments @('install', $archivePath) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or (Get-Content -Raw -LiteralPath $deployment).Trim() -ne 'signed') { throw "Trusted package did not install. $Output" }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Generate untrusted signing key' -Arguments @('keygen', $untrustedPrivateKey, $untrustedPublicKey) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Could not generate untrusted key. $Output" }
        $match = [regex]::Match($Output, '\b[0-9a-f]{64}\b')
        if (-not $match.Success) { throw 'keygen did not report the generated key identifier.' }
        $script:untrustedKeyId = $match.Value
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build package with untrusted signing key' -Arguments @('build', $untrustedSourceDir, $outputDir, '--sign', $untrustedPrivateKey) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or -not (Test-Path -LiteralPath $untrustedArchivePath)) { throw "Could not build untrusted-key package. $Output" }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject package signed by a second unknown key' -Arguments @('install', $untrustedArchivePath) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0) { throw 'Untrusted-key package was installed.' }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Trust and revoke second signing key' -Arguments @('trust', 'add', $untrustedPublicKey) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Could not trust the second key. $Output" }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Revoke trusted signing key' -Arguments @('trust', 'revoke', $untrustedKeyId) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Could not revoke the second key. $Output" }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject package signed by revoked key' -Arguments @('install', $untrustedArchivePath) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0) { throw 'Revoked-key package was installed.' }
    }

    $results += New-WpmManualStep -Name 'Create malformed, invalid, and unindexed signed package variants' -Action {
        $malformed = New-ArchiveVariant -Archive $archivePath -VariantName 'malformed-signature' -Mutate { param($dir) Set-Content -LiteralPath (Join-Path $dir '.wpm\signature.json') -Value '{not json}' }
        $invalid = New-ArchiveVariant -Archive $archivePath -VariantName 'invalid-signature' -Mutate { param($dir) (Get-Content -Raw -LiteralPath (Join-Path $dir '.wpm\signature.json')).Replace('"signature":', '"signature":"AAAA", "original_signature":') | Set-Content -LiteralPath (Join-Path $dir '.wpm\signature.json') }
        $unindexed = New-ArchiveVariant -Archive $archivePath -VariantName 'unindexed-entry' -Mutate { param($dir) Set-Content -LiteralPath (Join-Path $dir 'unexpected.txt') -Value 'unindexed' }
        $script:malformedArchive = $malformed
        $script:invalidArchive = $invalid
        $script:unindexedArchive = $unindexed
    }

    foreach ($variant in @(
        @{ Name = 'malformed signature metadata'; Archive = $malformedArchive },
        @{ Name = 'invalid signature'; Archive = $invalidArchive },
        @{ Name = 'unindexed archive entry'; Archive = $unindexedArchive }
    )) {
        $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name "Reject $($variant.Name)" -Arguments @('install', $variant.Archive) -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -eq 0) { throw "Package with $($variant.Name) was installed." }
        }
    }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Clear default signing key' -Arguments @('key', 'default', '--clear') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "Could not clear default signing key. $Output" }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build unsigned package' -Arguments @('build', $unsignedSourceDir, $outputDir) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or -not (Test-Path -LiteralPath $unsignedArchivePath)) { throw "Unsigned build failed. $Output" }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject unsigned package by default' -Arguments @('install', $unsignedArchivePath) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0) { throw 'Unsigned package was installed without an override.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Allow explicitly overridden unsigned package' -Arguments @('install', $unsignedArchivePath, '--allow-unsigned') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch '(?i)warning') { throw "Unsigned override did not succeed with a warning. $Output" }
    }
}
finally {
    $finished = Get-Date
    if ($EvidenceTex) { Write-WpmTestEvidence -TestCaseId 'TC-0012' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex }
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
    if ($null -eq $previousDataDir) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue } else { $env:WPM_DATA_DIR = $previousDataDir }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
