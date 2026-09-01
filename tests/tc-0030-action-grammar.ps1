param([Parameter(Mandatory=$true)][string]$WpmExe,[string]$EvidenceTex,[switch]$NoFailOnFailure)
$ErrorActionPreference='Stop'
$WpmExe=(Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')
$root=Join-Path ([IO.Path]::GetTempPath()) ("wpm-action-grammar-"+[Guid]::NewGuid().ToString('N'))
$previous=$env:WPM_DATA_DIR; $env:WPM_DATA_DIR=Join-Path $root 'must-not-exist'
$started=Get-Date; $results=@()
try {
    $cases=@(
        @{Command='repo'; Args=@('repo','unknown'); Detail='invalid action or operand count'},
        @{Command='repo'; Args=@('repo','remove','one','two'); Detail='invalid action or operand count'},
        @{Command='key'; Args=@('key','other','file'); Detail='expected default'},
        @{Command='key'; Args=@('key','default','one','two'); Detail='expected default'},
        @{Command='trust'; Args=@('trust','add','name','key.pub'); Detail='invalid action or operand count'},
        @{Command='trust'; Args=@('trust','revoke'); Detail='invalid action or operand count'},
        @{Command='config'; Args=@('config','other','prerelease'); Detail='invalid action, setting, value, or operand count'},
        @{Command='config'; Args=@('config','set','prerelease','maybe'); Detail='invalid action, setting, value, or operand count'},
        @{Command='config'; Args=@('config','unset','prerelease'); Detail='invalid action, setting, value, or operand count'}
    )
    foreach($case in $cases){$command=$case.Command;$detail=$case.Detail
        $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name "Reject invalid $command action shape" -Arguments $case.Args -Assert {
            param($ExitCode,$Output)
            if($ExitCode -eq 0 -or $Output -notmatch "Error: $detail" -or $Output -notmatch "Usage: wpm $command" -or
               $Output -notmatch "Run 'wpm help $command'" -or $Output -match 'Commands:'){throw "Action diagnostic was not narrow. $Output"}
        }
    }
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Show executable trust and config examples' -Arguments @('--help') -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0 -or $Output -notmatch 'wpm trust add \.\\maintainer\.public' -or
           $Output -notmatch 'wpm config set prerelease true --package my-package' -or
           $Output -match 'trust add maintainer'){throw "Task examples do not match the command grammar. $Output"}
    }
    $results+=New-WpmManualStep -Name 'Action validation does not initialize durable state' -Action {
        if(Test-Path -LiteralPath $env:WPM_DATA_DIR){throw 'Action validation initialized WPM state.'}; 'No WPM data directory was created.'
    }
} finally {
    $finished=Get-Date
    if($EvidenceTex){Write-WpmTestEvidence -TestCaseId 'TC-0030' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
    if($null -eq $previous){Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue}else{$env:WPM_DATA_DIR=$previous}
}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
