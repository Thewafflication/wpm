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
$packageName = "http-repository-$testId"
$crossOriginName = "http-cross-origin-$testId"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "wpm-network-repository-$testId"
$dataDir = Join-Path $testRoot 'wpm-data'
$sourceDir = Join-Path $testRoot 'source'
$repositoryDir = Join-Path $testRoot 'repository'
$deployment = Join-Path $testRoot 'deployment.txt'
$unc = "\\wpm-test-server\packages-$testId"
$previousDataDir = $env:WPM_DATA_DIR
$started = Get-Date
$results = @()
$server = $null
$archive = $null

try {
    $env:WPM_DATA_DIR = $dataDir
    New-Item -ItemType Directory -Force -Path $sourceDir, $repositoryDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $sourceDir '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\package.txt') -Value @(
        "name=$packageName"
        'version=1.0.0'
        'arch=any'
        'debug=false'
    )
    Set-Content -LiteralPath (Join-Path $sourceDir '.wpm\install.cmd') -Value @(
        '@echo off'
        "echo installed-from-http-repository> `"$deployment`""
    )
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build HTTP test package' -Arguments @('build', $sourceDir, $repositoryDir) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "HTTP test package build failed. $Output" }
    }
    $archive = Get-ChildItem -LiteralPath $repositoryDir -Filter '*.zip' | Select-Object -First 1
    if (-not $archive) { throw 'HTTP test package archive was not created.' }

    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    $listener.Start()
    $port = ([Net.IPEndPoint]$listener.LocalEndpoint).Port
    $listener.Stop()
    $httpRoot = "http://127.0.0.1:$port"
    Set-Content -NoNewline -LiteralPath (Join-Path $repositoryDir 'index.json') -Value (
        '{"version":1,"packages":[{"name":"' + $packageName +
        '","version":"1.0.0","arch":"any","url":"' + $archive.Name +
        '"},{"name":"' + $crossOriginName +
        '","version":"1.0.0","arch":"any","url":"http://localhost:' +
        $port + '/' + $archive.Name + '"}]}'
    )
    $python = (Get-Command python -ErrorAction Stop).Source
    $server = Start-Process -FilePath $python -ArgumentList @(
        '-m', 'http.server', $port, '--bind', '127.0.0.1'
    ) -WorkingDirectory $repositoryDir -WindowStyle Hidden -PassThru
    $ready = $false
    for ($attempt = 0; $attempt -lt 40 -and -not $ready; $attempt++) {
        try {
            $client = [Net.Sockets.TcpClient]::new('127.0.0.1', $port)
            $client.Dispose()
            $ready = $true
        }
        catch { Start-Sleep -Milliseconds 100 }
    }
    if (-not $ready) { throw 'Loopback HTTP test server did not start.' }

    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject HTTP without explicit opt-in' -Arguments @('repo', 'add', $httpRoot) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'require --allow-insecure-http') {
            throw "Plain HTTP was not disabled by default. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Persist repository-scoped HTTP opt-in' -Arguments @('repo', 'add', $httpRoot, '--priority', '9', '--allow-insecure-http') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch 'Warning: insecure HTTP transport') {
            throw "HTTP opt-in or warning failed. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'List HTTP policy and effective locator' -Arguments @('repo', 'list') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch [regex]::Escape($httpRoot) -or
            $Output -notmatch 'insecure HTTP allowed') {
            throw "HTTP policy was not visible after reload. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Refresh opted-in HTTP repository' -Arguments @('repo', 'update') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch 'Warning: using insecure HTTP transport' -or
            $Output -notmatch [regex]::Escape($httpRoot)) {
            throw "HTTP refresh did not warn and identify its source. $Output"
        }
        $outputLines = @($Output -split '\r?\n')
        $updatedLine = "Updated repository index: $httpRoot"
        $updatedIndexes = @(for ($i = 0; $i -lt $outputLines.Count; $i++) {
            if ($outputLines[$i] -eq $updatedLine) { $i }
        })
        if ($updatedIndexes.Count -ne 1 -or $updatedIndexes[0] -lt 2) {
            throw "Known-length HTTP progress was not concise and stable. $Output"
        }
        $updatedIndex = $updatedIndexes[0]
        $startMatch = [regex]::Match($outputLines[$updatedIndex - 2],
            '^Download progress: repository index: 0% \(0/([1-9][0-9]*) bytes\)$')
        $completionMatch = [regex]::Match($outputLines[$updatedIndex - 1],
            '^Downloaded repository index: ([1-9][0-9]*) bytes$')
        if (-not $startMatch.Success -or -not $completionMatch.Success -or
            $startMatch.Groups[1].Value -ne $completionMatch.Groups[1].Value) {
            throw "Loopback HTTP progress did not report one stable start/completion sequence. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Retain signature policy over HTTP' -Arguments @('install', $packageName) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0) { throw 'Unsigned HTTP package was trusted implicitly.' }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Install from opted-in HTTP repository' -Arguments @('install', $packageName, '--allow-unsigned') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0) { throw "HTTP package installation failed. $Output" }
        if ((Get-Content -Raw -LiteralPath $deployment).Trim() -ne 'installed-from-http-repository') {
            throw 'HTTP package install script did not run.'
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject cross-origin HTTP package URL' -Arguments @('install', $crossOriginName, '--allow-unsigned') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'invalid package URL') {
            throw "Cross-origin HTTP package URL was not rejected. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject and redact URL credentials in verbose mode' -Arguments @('repo', 'add', "http://user:secret@127.0.0.1:$port", '--allow-insecure-http', '--verbose') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'Verbose:' -or
            $Output -match 'user:secret' -or $Output -match 'secret@') {
            throw "Credential-bearing URL was accepted, verbose mode was not active, or credentials were disclosed. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject HTTP opt-in on HTTPS' -Arguments @('repo', 'add', 'https://packages.example.test', '--allow-insecure-http') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'valid only for an http://') {
            throw "HTTP permission was not repository-transport scoped. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Accept canonical UNC repository locator' -Arguments @('repo', 'add', $unc, '--priority', '3') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch [regex]::Escape($unc)) {
            throw "Valid UNC locator was rejected. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Report unavailable UNC source without credential prompts' -Arguments @('repo', 'update', '--offline') -Assert {
        param($ExitCode, $Output)
        if ($Output -notmatch [regex]::Escape($unc) -or $Output -notmatch 'could not read filesystem repository index') {
            throw "Unavailable UNC source was not identified safely. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Reject incomplete UNC locator' -Arguments @('repo', 'add', '\\wpm-test-server') -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -eq 0 -or $Output -notmatch 'require a server and share') {
            throw "Incomplete UNC locator was accepted. $Output"
        }
    }
    $results += Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Remove UNC repository' -Arguments @('repo', 'remove', $unc) -Assert {
        param($ExitCode, $Output)
        if ($ExitCode -ne 0 -or $Output -notmatch 'Repository removed') {
            throw "UNC repository removal failed. $Output"
        }
    }
}
finally {
    $finished = Get-Date
    if ($server -and -not $server.HasExited) { Stop-Process -Id $server.Id -Force }
    if ($EvidenceTex) {
        Write-WpmTestEvidence -TestCaseId 'TC-0025' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex
    }
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
    if ($null -eq $previousDataDir) { Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue }
    else { $env:WPM_DATA_DIR = $previousDataDir }
}

Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
