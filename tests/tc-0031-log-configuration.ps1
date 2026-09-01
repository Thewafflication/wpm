param([Parameter(Mandatory=$true)][string]$WpmExe,[string]$EvidenceTex,[switch]$NoFailOnFailure)
$ErrorActionPreference='Stop'
$WpmExe=(Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')
$root=Join-Path ([IO.Path]::GetTempPath()) ("wpm-log-config-"+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$previousData=$env:WPM_DATA_DIR; $previousFile=$env:WPM_LOG_FILE; $previousLevel=$env:WPM_LOG_LEVEL
$env:WPM_DATA_DIR=Join-Path $root 'data'
$started=Get-Date; $results=@()
try {
    foreach($level in @('normal','verbose')) {
        $log=Join-Path $root "$level.log"
        $env:WPM_LOG_FILE=$log; $env:WPM_LOG_LEVEL=$level
        $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name "Retain failure at $level log level" -Arguments @('verify',(Join-Path $root 'missing.wpm'),'--verbose') -Assert {
            param($ExitCode,$Output)
            if($ExitCode -eq 0){throw 'Missing package unexpectedly verified.'}
        }
        $results+=New-WpmManualStep -Name "Inspect $level operational log" -Action {
            if(!(Test-Path -LiteralPath $log)){throw "Custom log was not created: $log"}
            $content=Get-Content -LiteralPath $log -Raw
            if($content -notmatch '\[[0-9]{4}-[0-9]{2}-[0-9]{2}T' -or $content -notmatch '\[ERROR\].*could not open package archive'){
                throw "Timestamped failure evidence is absent from $level log. $content"
            }
            $hasVerbose=$content -match '\[DEBUG\].*Verifying archive:'
            if(($level -eq 'verbose') -ne $hasVerbose){throw "Unexpected debug filtering at $level level. $content"}
            "Custom $level log retained the required records."
        }
    }
    $invalidLog=Join-Path $root 'invalid.log'
    $env:WPM_LOG_FILE=$invalidLog; $env:WPM_LOG_LEVEL='everything'
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Warn and continue for invalid log level' -Arguments @('verify',(Join-Path $root 'missing.wpm')) -Assert {
        param($ExitCode,$Output)
        if($ExitCode -eq 0 -or $Output -notmatch 'could not initialize its operational log'){
            throw "Invalid-level recovery was not actionable. $Output"
        }
    }
    $results+=New-WpmManualStep -Name 'Invalid level creates no persistent log' -Action {
        if(Test-Path -LiteralPath $invalidLog){throw 'Invalid log level created a log.'}; 'No invalid-level log was created.'
    }
} finally {
    $finished=Get-Date
    if($EvidenceTex){Write-WpmTestEvidence -TestCaseId 'TC-0031' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
    if($null -eq $previousData){Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue}else{$env:WPM_DATA_DIR=$previousData}
    if($null -eq $previousFile){Remove-Item Env:WPM_LOG_FILE -ErrorAction SilentlyContinue}else{$env:WPM_LOG_FILE=$previousFile}
    if($null -eq $previousLevel){Remove-Item Env:WPM_LOG_LEVEL -ErrorAction SilentlyContinue}else{$env:WPM_LOG_LEVEL=$previousLevel}
}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
