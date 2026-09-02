param([Parameter(Mandatory=$true)][string]$WpmExe,[string]$EvidenceTex,[switch]$NoFailOnFailure)
$ErrorActionPreference='Stop'
$WpmExe=(Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$id=[Guid]::NewGuid().ToString('N')
$root=Join-Path ([IO.Path]::GetTempPath()) ("wpm-verbose-details-"+$id)
$data=Join-Path $root 'data'
$packages=Join-Path $root 'packages'
$source=Join-Path $root 'source'
$pkgName="verbose-details-$id"
$secret="environment-secret-$id"
$previousData=$env:WPM_DATA_DIR
$previousSecret=$env:WPM_TEST_SECRET
$env:WPM_DATA_DIR=$data
$env:WPM_TEST_SECRET=$secret
$started=Get-Date; $results=@()

try {
    New-Item -ItemType Directory -Force -Path $packages,(Join-Path $source '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $source '.wpm\package.txt') -Value @(
        "name=$pkgName","version=1.0.0","arch=any","debug=false")
    Set-Content -LiteralPath (Join-Path $source '.wpm\install.cmd') -Value @('@echo off','echo installed')
    Set-Content -LiteralPath (Join-Path $source '.wpm\remove.cmd') -Value @('@echo off','echo removed')

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build verbose-details package' -Arguments @('build',$source,$packages) -Assert {
        param($ExitCode,$Output) if($ExitCode -ne 0){throw "build failed. $Output"} }
    $archive=(Get-ChildItem -LiteralPath $packages -Filter "$pkgName-*.zip" | Select-Object -First 1).FullName
    if(-not $archive){throw 'Package archive was not created.'}

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Report detailed verbose install lifecycle without environment secrets' -Arguments @('install',$archive,'--allow-unsigned','--verbose') -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0){throw "Verbose install failed. $Output"}
        foreach($expected in @(
            'Verbose: Installing archive:',
            'Verbose: Using staging directory:',
            "Verbose: Resolved package identity: $pkgName any 1.0.0",
            'Verbose: Verifying package index:',
            'Verbose: Running install script:',
            'Verbose: Storing archive:',
            "--- end package ${pkgName}: install script output (exit code 0) ---",
            "Result: $pkgName any installed")) {
            if($Output -notmatch [regex]::Escape($expected)){throw "Verbose install omitted '$expected'. $Output"}
        }
        if($Output -match [regex]::Escape($secret)){throw "Verbose install disclosed an unrelated environment secret. $Output"}
    }

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Report detailed verbose removal lifecycle without environment secrets' -Arguments @('remove',"$pkgName-any-1.0.0",'--verbose') -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0){throw "Verbose removal failed. $Output"}
        foreach($expected in @(
            'Verbose: Removing archive:',
            'Verbose: Using removal staging directory:',
            "Verbose: Resolved package identity: $pkgName any 1.0.0",
            'Verbose: Verifying package index:',
            'Verbose: Running removal script:',
            'Verbose: Deleting retained archive:',
            "--- end package ${pkgName}-any-1.0.0: removal script output (exit code 0) ---",
            "Result: $pkgName any removed")) {
            if($Output -notmatch [regex]::Escape($expected)){throw "Verbose removal omitted '$expected'. $Output"}
        }
        if($Output -match [regex]::Escape($secret)){throw "Verbose removal disclosed an unrelated environment secret. $Output"}
    }
} finally {
    $finished=Get-Date
    if($EvidenceTex){Write-WpmTestEvidence -TestCaseId 'TC-0035' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
    if($null -eq $previousData){Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue}else{$env:WPM_DATA_DIR=$previousData}
    if($null -eq $previousSecret){Remove-Item Env:WPM_TEST_SECRET -ErrorAction SilentlyContinue}else{$env:WPM_TEST_SECRET=$previousSecret}
}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
