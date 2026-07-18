param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,

    [string]$EvidenceTex,

    [switch]$NoFailOnFailure
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$setupCmd = Join-Path $repositoryRoot 'setup.cmd'
$removeCmd = Join-Path $repositoryRoot 'remove.cmd'
$installCmd = Join-Path $repositoryRoot 'install.cmd'
$testId = [Guid]::NewGuid().ToString('N')
$installDir = Join-Path ([IO.Path]::GetTempPath()) "wpm-self-install-$testId"
$dataDir = Join-Path ([IO.Path]::GetTempPath()) "wpm-self-data-$testId"
$environmentRegistryPath = "HKCU:\Software\WPM\Tests\$testId"
$environmentRegistryKey = "HKCU\Software\WPM\Tests\$testId"

function Invoke-CmdScript {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Script,

        [string[]]$Arguments = @()
    )

    & $Script @Arguments 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "$(Split-Path -Leaf $Script) exited $LASTEXITCODE."
    }
}

function Test-WpmProcessIsElevated {
    $groups = & whoami /groups /fo csv /nh 2>$null | Out-String
    return $groups -match 'S-1-16-(12288|16384)'
}

function Get-WpmTestEnvironment {
    $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey("Software\WPM\Tests\$testId")
    try {
        return [pscustomobject]@{
            WPM = $key.GetValue('WPM', $null)
            WPM_DATA_DIR = $key.GetValue('WPM_DATA_DIR', $null)
            Path = $key.GetValue(
                'Path',
                $null,
                [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames
            )
        }
    } finally {
        if ($null -ne $key) { $key.Dispose() }
    }
}

$started = Get-Date
$results = @()
$expectedDefaultScope = if (Test-WpmProcessIsElevated) { 'machine' } else { 'user' }
$previousInstallDir = $env:WPM_INSTALL_DIR
$previousDataDir = $env:WPM_DATA_DIR
$previousEnvironmentRegistryKey = $env:WPM_ENVIRONMENT_REGISTRY_KEY

try {
    $env:WPM_INSTALL_DIR = $installDir
    $env:WPM_DATA_DIR = $dataDir
    $env:WPM_ENVIRONMENT_REGISTRY_KEY = $environmentRegistryKey
    New-Item -Path $environmentRegistryPath -Force | Out-Null
    New-ItemProperty -Path $environmentRegistryPath -Name Path -PropertyType ExpandString -Value 'C:\Windows' -Force | Out-Null

    $results += New-WpmManualStep -Name 'Verify native Program Files selection' -Action {
        if ((Get-Content -Raw -LiteralPath $setupCmd) -notmatch 'ProgramW6432') {
            throw 'setup.cmd does not prefer the native Program Files directory.'
        }
        if ((Get-Content -Raw -LiteralPath $setupCmd) -notmatch 'HKCU\\Environment' -or
            (Get-Content -Raw -LiteralPath $setupCmd) -notmatch 'LocalAppData') {
            throw 'setup.cmd does not provide a user-scoped installation mode.'
        }
    }

    $results += New-WpmManualStep -Name 'Validate latest-release bootstrap installer contract' -Action {
        $help = & $installCmd --help 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $help -notmatch '(?i)latest WPM release') {
            throw 'install.cmd --help did not complete successfully.'
        }
        $bootstrap = Get-Content -Raw -LiteralPath $installCmd
        foreach ($pattern in @(
            'releases/latest/download',
            'index\.json',
            'PROCESSOR_ARCHITEW6432',
            'wpm-release\.public',
            'WPM_DATA_DIR',
            'trust add',
            'verify \$archive',
            '& \$setup \$wpm',
            'Remove-Item -LiteralPath \$work -Recurse -Force'
        )) {
            if ($bootstrap -notmatch $pattern) { throw "install.cmd is missing bootstrap behavior: $pattern" }
        }
        'Latest-release bootstrap selection, validation, setup, and cleanup are configured.'
    }

    $results += New-WpmManualStep -Name 'Automatically select the installation scope from elevation' -Action {
        Invoke-CmdScript -Script $setupCmd -Arguments @($WpmExe)
        if (-not (Test-Path -LiteralPath (Join-Path $installDir 'wpm.exe') -PathType Leaf)) {
            throw "setup.cmd did not install wpm.exe to $installDir"
        }
        $environment = Get-WpmTestEnvironment
        if ($environment.WPM -ne $installDir -or $environment.Path -notmatch [regex]::Escape('%WPM%')) {
            throw 'Default setup.cmd did not configure the persistent environment entries.'
        }
        if ($expectedDefaultScope -eq 'user' -and $environment.WPM_DATA_DIR -ne $dataDir) {
            throw 'Non-elevated setup.cmd did not select user scope.'
        }
        if ($expectedDefaultScope -eq 'machine' -and $null -ne $environment.WPM_DATA_DIR) {
            throw 'Elevated setup.cmd did not select machine scope.'
        }
        Invoke-CmdScript -Script $removeCmd
        if (Test-Path -LiteralPath $installDir) {
            throw 'Default remove.cmd did not remove the installation directory.'
        }
    }

    $results += New-WpmManualStep -Name 'Install WPM with setup.cmd --user' -Action {
        Invoke-CmdScript -Script $setupCmd -Arguments @('--user', $WpmExe)
        if (-not (Test-Path -LiteralPath (Join-Path $installDir 'wpm.exe') -PathType Leaf)) {
            throw "setup.cmd did not install wpm.exe to $installDir"
        }
        foreach ($relativePath in @(
            'README.md',
            'LICENSE.txt',
            'THIRD_PARTY_NOTICES.md',
            'docs\usage.md'
        )) {
            $installedFile = Join-Path $installDir $relativePath
            if (-not (Test-Path -LiteralPath $installedFile -PathType Leaf)) {
                throw "setup.cmd did not install $relativePath to $installDir"
            }
        }
        $sourceDocumentation = Get-ChildItem -LiteralPath (Join-Path $repositoryRoot 'docs') -Filter '*.md' -File
        foreach ($sourceFile in $sourceDocumentation) {
            $installedFile = Join-Path (Join-Path $installDir 'docs') $sourceFile.Name
            if (-not (Test-Path -LiteralPath $installedFile -PathType Leaf)) {
                throw "setup.cmd did not install documentation file $($sourceFile.Name)"
            }
        }
        if (Test-Path -LiteralPath $dataDir) {
            throw 'setup.cmd must not initialize WPM data directories.'
        }
        $environment = Get-WpmTestEnvironment
        if ($environment.WPM -ne $installDir) {
            throw 'setup.cmd did not create the persistent WPM environment variable.'
        }
        if ($environment.WPM_DATA_DIR -ne $dataDir) {
            throw 'setup.cmd --user did not create the persistent WPM_DATA_DIR environment variable.'
        }
        if ($environment.Path -notmatch [regex]::Escape('%WPM%')) {
            throw 'setup.cmd did not add %WPM% to the persistent Path.'
        }
    }

    $results += New-WpmManualStep -Name 'Repeat user-scoped WPM installation' -Action {
        Invoke-CmdScript -Script $setupCmd -Arguments @('--user', $WpmExe)
        $environment = Get-WpmTestEnvironment
        $pathEntries = [regex]::Matches([string]$environment.Path, [regex]::Escape('%WPM%')).Count
        if ($pathEntries -ne 1) {
            throw "Repeated setup created $pathEntries %WPM% Path entries."
        }
    }

    $results += New-WpmManualStep -Name 'Discover self-installed WPM by name in a new command process' -Action {
        $environment = Get-WpmTestEnvironment
        $previousProcessWpm = $env:WPM
        $previousProcessDataDir = $env:WPM_DATA_DIR
        $previousProcessPath = $env:Path
        try {
            $env:WPM = $environment.WPM
            $env:WPM_DATA_DIR = $environment.WPM_DATA_DIR
            $env:Path = ([string]$environment.Path).Replace('%WPM%', $environment.WPM) + ';' + $previousProcessPath
            $output = & $env:ComSpec /d /c 'wpm --version' 2>&1 | Out-String
            if ($LASTEXITCODE -ne 0) {
                throw "wpm --version in a new command process exited $LASTEXITCODE. Output: $output"
            }
            if ($output -notmatch 'Waughtal Package Manager .* Version ') {
                throw 'The new command process did not resolve wpm by name.'
            }
        }
        finally {
            $env:WPM = $previousProcessWpm
            $env:WPM_DATA_DIR = $previousProcessDataDir
            $env:Path = $previousProcessPath
        }
    }

    $installedExe = Join-Path $installDir 'wpm.exe'
    $results += Invoke-WpmTestStep `
        -WpmExe $installedExe `
        -Name 'Initialize WPM data directories with a local repository command' `
        -Arguments @('repo', 'list') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            foreach ($directory in @($dataDir, (Join-Path $dataDir 'packages'), (Join-Path $dataDir 'temp'), (Join-Path $dataDir 'cache'), (Join-Path $dataDir 'config'))) {
                if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
                    throw "The executable did not initialize $directory"
                }
            }
        }

    $results += New-WpmManualStep -Name 'Remove WPM with remove.cmd --user' -Action {
        Invoke-CmdScript -Script $removeCmd -Arguments @('--user')
        if (Test-Path -LiteralPath $installDir) {
            throw "remove.cmd did not remove $installDir"
        }
        if (Test-Path -LiteralPath $dataDir) {
            throw "remove.cmd did not remove $dataDir"
        }
        $environment = Get-WpmTestEnvironment
        if ($null -ne $environment.WPM -or $null -ne $environment.WPM_DATA_DIR -or
            $environment.Path -match [regex]::Escape('%WPM%')) {
            throw 'remove.cmd did not remove the persistent WPM environment entries.'
        }
    }
}
finally {
    $env:WPM_INSTALL_DIR = $previousInstallDir
    $env:WPM_DATA_DIR = $previousDataDir
    $env:WPM_ENVIRONMENT_REGISTRY_KEY = $previousEnvironmentRegistryKey
    $finished = Get-Date
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0007' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $installDir) {
        Remove-Item -LiteralPath $installDir -Recurse -Force
    }
    if (Test-Path -LiteralPath $dataDir) {
        Remove-Item -LiteralPath $dataDir -Recurse -Force
    }
    if (Test-Path -LiteralPath $environmentRegistryPath) {
        Remove-Item -LiteralPath $environmentRegistryPath -Recurse -Force
    }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
