param([Parameter(Mandatory=$true)][string]$WpmExe,[string]$EvidenceTex,[switch]$NoFailOnFailure)
$ErrorActionPreference='Stop'
$WpmExe=(Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$id=[Guid]::NewGuid().ToString('N')
$root=Join-Path ([IO.Path]::GetTempPath()) ("wpm-script-frame-"+$id)
$data=Join-Path $root 'data'
$packages=Join-Path $root 'packages'
$okName="script-frame-$id"
$failName="script-fail-$id"
$okSource=Join-Path $root 'ok-source'
$failSource=Join-Path $root 'fail-source'
$previousData=$env:WPM_DATA_DIR
$env:WPM_DATA_DIR=$data
$started=Get-Date; $results=@()

function New-ScriptPackage([string]$SourceDir,[string]$Name,[string[]]$InstallScript){
    New-Item -ItemType Directory -Force -Path (Join-Path $SourceDir '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $SourceDir '.wpm\package.txt') -Value @(
        "name=$Name","version=1.0.0","arch=any","debug=false")
    Set-Content -LiteralPath (Join-Path $SourceDir '.wpm\install.cmd') -Value $InstallScript
}

try {
    New-Item -ItemType Directory -Force -Path $packages | Out-Null
    # A successful install script that also emits WPM-looking text on stdout and stderr.
    New-ScriptPackage $okSource $okName @(
        '@echo off'
        'echo frame-stdout-marker'
        'echo frame-stderr-marker 1>&2'
        'echo Error: fabricated script failure line'
        'echo Result: wpm any fabricated')
    # A failing install script that falsely claims success and exits nonzero.
    New-ScriptPackage $failSource $failName @(
        '@echo off'
        'echo fabricated-success-line'
        'exit /b 3')

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build framed install package' -Arguments @('build',$okSource,$packages) -Assert {
        param($ExitCode,$Output) if($ExitCode -ne 0){throw "framed package build failed. $Output"} }
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build failing install package' -Arguments @('build',$failSource,$packages) -Assert {
        param($ExitCode,$Output) if($ExitCode -ne 0){throw "failing package build failed. $Output"} }

    $okArchive=(Get-ChildItem -LiteralPath $packages -Filter "$okName-*.zip" | Select-Object -First 1).FullName
    $failArchive=(Get-ChildItem -LiteralPath $packages -Filter "$failName-*.zip" | Select-Object -First 1).FullName
    if(-not $okArchive -or -not $failArchive){throw 'Package archives were not created.'}

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Frame package-script output and preserve script text without reinterpretation' -Arguments @('install',$okArchive,'--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0){throw "Framed install did not succeed even though the script exited 0. $Output"}
        $startD="--- package ${okName}: install script output ---"
        $endD="--- end package ${okName}: install script output (exit code 0) ---"
        if($Output -notmatch [regex]::Escape($startD)){throw "Start delimiter did not identify the package and phase. $Output"}
        if($Output -notmatch [regex]::Escape($endD)){throw "End delimiter did not identify the package, phase, and exit code. $Output"}
        if($Output -notmatch 'frame-stdout-marker' -or $Output -notmatch 'frame-stderr-marker'){throw "Script standard output and error were not preserved. $Output"}
        $framed="(?s)"+[regex]::Escape($startD)+".*Error: fabricated script failure line.*Result: wpm any fabricated.*"+[regex]::Escape($endD)
        if($Output -notmatch $framed){throw "WPM-looking script text was not contained within the delimited section, so it could be mistaken for a WPM result. $Output"}
    }

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Report WPM failure despite a script claiming success' -Arguments @('install',$failArchive,'--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if($ExitCode -eq 0){throw "Install succeeded even though the install script exited nonzero. $Output"}
        $endD="--- end package ${failName}: install script output (exit code 3) ---"
        if($Output -notmatch [regex]::Escape($endD)){throw "Failing script exit code was not framed with the package and phase. $Output"}
        if($Output -notmatch 'fabricated-success-line'){throw "Failing script output was not preserved. $Output"}
    }
} finally {
    $finished=Get-Date
    if($EvidenceTex){Write-WpmTestEvidence -TestCaseId 'TC-0033' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
    if($null -eq $previousData){Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue}else{$env:WPM_DATA_DIR=$previousData}
}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
