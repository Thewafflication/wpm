param([Parameter(Mandatory=$true)][string]$WpmExe,[string]$EvidenceTex,[switch]$NoFailOnFailure)
$ErrorActionPreference='Stop'
$WpmExe=(Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')
$root=Join-Path ([IO.Path]::GetTempPath()) ("wpm-failure-log-"+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$log=Join-Path $root 'operation.log'; $data=Join-Path $root 'data'
$previousData=$env:WPM_DATA_DIR; $previousFile=$env:WPM_LOG_FILE; $previousLevel=$env:WPM_LOG_LEVEL
$env:WPM_DATA_DIR=$data; $env:WPM_LOG_FILE=$log; $env:WPM_LOG_LEVEL='normal'
$started=Get-Date; $results=@()
try {
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Point operational failure to active log once' -Arguments @('verify',(Join-Path $root 'missing.wpm')) -Assert {
        param($ExitCode,$Output)
        $expected="Operational log: $log"
        if($ExitCode -eq 0 -or ([regex]::Matches($Output,[regex]::Escape($expected))).Count -ne 1){throw "Failure log guidance was absent or repeated. $Output"}
        if(!(Test-Path -LiteralPath $log) -or (Get-Content -LiteralPath $log -Raw) -notmatch [regex]::Escape($expected)){throw 'Failure log did not retain its guidance record.'}
    }
    Remove-Item Env:WPM_LOG_FILE -ErrorAction SilentlyContinue
    $env:WPM_DATA_DIR=Join-Path $root 'early-must-not-exist'
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Do not claim a log for early invalid input' -Arguments @('verify','--unknown') -Assert {
        param($ExitCode,$Output)
        if($ExitCode -eq 0 -or $Output -match 'Operational log:'){throw "Early failure claimed an operational log. $Output"}
        if(Test-Path -LiteralPath $env:WPM_DATA_DIR){throw 'Early failure initialized durable state.'}
    }
} finally {
    $finished=Get-Date
    if($EvidenceTex){Write-WpmTestEvidence -TestCaseId 'TC-0032' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
    if($null -eq $previousData){Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue}else{$env:WPM_DATA_DIR=$previousData}
    if($null -eq $previousFile){Remove-Item Env:WPM_LOG_FILE -ErrorAction SilentlyContinue}else{$env:WPM_LOG_FILE=$previousFile}
    if($null -eq $previousLevel){Remove-Item Env:WPM_LOG_LEVEL -ErrorAction SilentlyContinue}else{$env:WPM_LOG_LEVEL=$previousLevel}
}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
