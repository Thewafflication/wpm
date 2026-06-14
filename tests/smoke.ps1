param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
$testId = [Guid]::NewGuid().ToString('N')
$packageName = "wpm-smoke-$testId"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-tests-$testId"
$sourceDir = Join-Path $testRoot $packageName
$outputDir = Join-Path $testRoot 'packages'
$installDir = Join-Path 'C:\TEMP' $packageName

function Invoke-Wpm {
    param([string[]]$Arguments)

    $output = & $WpmExe @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "wpm $($Arguments -join ' ') failed with exit code $LASTEXITCODE`n$output"
    }
    return ($output -join "`n")
}

function Invoke-WpmExpectFailure {
    param([string[]]$Arguments)

    $output = & $WpmExe @Arguments 2>&1
    if ($LASTEXITCODE -eq 0) {
        throw "wpm $($Arguments -join ' ') unexpectedly succeeded.`n$output"
    }
    return ($output -join "`n")
}

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

try {
    New-Item -ItemType Directory -Force -Path $sourceDir, $outputDir | Out-Null

    $versionOutput = Invoke-Wpm @('--version')
    if ($versionOutput -notmatch 'Waughtal Package Manager .* Version ') {
        throw 'Version output did not contain the WPM version.'
    }

    $verboseOutput = Invoke-Wpm @('--verbose')
    if ($verboseOutput -notmatch 'miniz .+commit ') {
        throw 'Verbose output did not contain miniz version information.'
    }

    Push-Location $sourceDir
    try {
        Invoke-WpmExpectFailure @('init', 'bad name') | Out-Null
        if (Test-Path -LiteralPath '.wpm') {
            throw 'invalid init unexpectedly created .wpm.'
        }

        $initOutput = Invoke-Wpm @('init', $packageName)
        if ($initOutput -match 'Updating') {
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
            throw 'init did not write the requested package name.'
        }

        Assert-FileContent '.wpm\index.csv' 'filename,crc'
        Set-Content -LiteralPath '.wpm\package.txt' -Value 'preserve=this'
        Invoke-Wpm @('init', $packageName) | Out-Null
        Assert-FileContent '.wpm\package.txt' 'preserve=this'

        New-Item -ItemType Directory -Force -Path 'nested' | Out-Null
        Set-Content -LiteralPath 'hello.txt' -Value 'hello from wpm'
        Set-Content -LiteralPath 'nested\data.txt' -Value 'nested package data'
    }
    finally {
        Pop-Location
    }

    Invoke-Wpm @('build', $sourceDir, $outputDir, '--no-index') | Out-Null
    $archivePath = Join-Path $outputDir "$packageName.zip"
    if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
        throw "build did not create $archivePath"
    }

    Invoke-Wpm @('install', $archivePath) | Out-Null
    Assert-FileContent (Join-Path $installDir 'hello.txt') 'hello from wpm'
    Assert-FileContent (Join-Path $installDir 'nested\data.txt') 'nested package data'

    Write-Output 'WPM smoke tests passed.'
}
finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $installDir) {
        Remove-Item -LiteralPath $installDir -Recurse -Force
    }
}
