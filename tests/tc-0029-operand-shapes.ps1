param([Parameter(Mandatory=$true)][string]$WpmExe,[string]$EvidenceTex,[switch]$NoFailOnFailure)
$ErrorActionPreference='Stop'
$WpmExe=(Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')
$root=Join-Path ([IO.Path]::GetTempPath()) ("wpm-operand-shapes-"+[Guid]::NewGuid().ToString('N'))
$previous=$env:WPM_DATA_DIR
$env:WPM_DATA_DIR=Join-Path $root 'must-not-exist'
$started=Get-Date
$results=@()
try {
    $cases=@(
        @{ Command='build'; Args=@('build'); Detail='missing required source directory' },
        @{ Command='build'; Args=@('build','one','two','three'); Detail='too many operands' },
        @{ Command='verify'; Args=@('verify'); Detail='missing required package operand' },
        @{ Command='install'; Args=@('install','--offline'); Detail='missing required package operand' },
        @{ Command='remove'; Args=@('remove'); Detail='missing required package operand' },
        @{ Command='keygen'; Args=@('keygen','private'); Detail='missing required key-file operand' },
        @{ Command='keygen'; Args=@('keygen','private','public','extra'); Detail='too many operands' },
        @{ Command='update'; Args=@('update','extra'); Detail='unexpected operand' },
        @{ Command='init'; Args=@('init','one','two'); Detail='too many operands' }
    )
    foreach($case in $cases){
        $command=$case.Command; $detail=$case.Detail
        $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name "Reject $command operand shape" -Arguments $case.Args -Assert {
            param($ExitCode,$Output)
            if($ExitCode -eq 0 -or $Output -notmatch "Error: $detail for $command" -or
               $Output -notmatch "Usage: wpm $command" -or $Output -notmatch "Run 'wpm help $command'" -or
               $Output -match 'Commands:'){throw "Operand diagnostic was not narrow. $Output"}
        }
    }
    $results+=New-WpmManualStep -Name 'Operand validation does not initialize durable state' -Action {
        if(Test-Path -LiteralPath $env:WPM_DATA_DIR){throw 'Operand validation initialized WPM state.'}
        'No WPM data directory was created.'
    }
} finally {
    $finished=Get-Date
    if($EvidenceTex){Write-WpmTestEvidence -TestCaseId 'TC-0029' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
    if($null -eq $previous){Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue}else{$env:WPM_DATA_DIR=$previous}
}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
