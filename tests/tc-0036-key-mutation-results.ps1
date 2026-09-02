param([Parameter(Mandatory=$true)][string]$WpmExe,[string]$EvidenceTex,[switch]$NoFailOnFailure)
$ErrorActionPreference='Stop'
$WpmExe=(Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$id=[Guid]::NewGuid().ToString('N')
$root=Join-Path ([IO.Path]::GetTempPath()) ("wpm-key-results-"+$id)
$data=Join-Path $root 'data'
$privateKey=Join-Path $root 'test.private'
$publicKey=Join-Path $root 'test.public'
$previousData=$env:WPM_DATA_DIR
$env:WPM_DATA_DIR=$data
$started=Get-Date; $results=@(); $keyId=$null

function Assert-FinalResult([string]$Output,[string]$Expected){
    $lines=@($Output -split '\r?\n')
    $matches=@($lines | Where-Object { $_ -eq $Expected })
    $last=@($lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })[-1]
    if($matches.Count -ne 1 -or $last -ne $Expected){throw "Expected exactly one final '$Expected' result. $Output"}
}

try {
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Key generation ends with a public-ID result' -Arguments @('keygen',$privateKey,$publicKey) -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0 -or $Output -match 'secret-key='){throw "Key generation failed or disclosed private material. $Output"}
        $match=[regex]::Match($Output,'(?m)^Result: signing key generated ([0-9a-f]{64})$')
        if(-not $match.Success){throw "Key generation omitted its public-ID result. $Output"}
        $script:keyId=$match.Groups[1].Value
        Assert-FinalResult $Output "Result: signing key generated $keyId"
    }
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Default-key configuration ends with an operation result' -Arguments @('key','default',$privateKey) -Assert {
        param($ExitCode,$Output) if($ExitCode -ne 0){throw "Default-key configuration failed. $Output"}; Assert-FinalResult $Output 'Result: default signing key configured' }
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Trust add ends with a public-ID result' -Arguments @('trust','add',$publicKey) -Assert {
        param($ExitCode,$Output) if($ExitCode -ne 0){throw "Trust add failed. $Output"}; Assert-FinalResult $Output "Result: trust active $keyId" }
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Idempotent trust add retains the same result' -Arguments @('trust','add',$publicKey) -Assert {
        param($ExitCode,$Output) if($ExitCode -ne 0 -or $Output -notmatch 'already trusted'){throw "Idempotent trust add failed. $Output"}; Assert-FinalResult $Output "Result: trust active $keyId" }
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Trust revocation ends with a public-ID result' -Arguments @('trust','revoke',$keyId) -Assert {
        param($ExitCode,$Output) if($ExitCode -ne 0){throw "Trust revocation failed. $Output"}; Assert-FinalResult $Output "Result: trust revoked $keyId" }
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Default-key clear ends with an operation result' -Arguments @('key','default','--clear') -Assert {
        param($ExitCode,$Output) if($ExitCode -ne 0){throw "Default-key clear failed. $Output"}; Assert-FinalResult $Output 'Result: default signing key cleared' }
} finally {
    $finished=Get-Date
    if($EvidenceTex){Write-WpmTestEvidence -TestCaseId 'TC-0036' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
    if($null -eq $previousData){Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue}else{$env:WPM_DATA_DIR=$previousData}
}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
