param([Parameter(Mandatory=$true)][string]$WpmExe,[string]$EvidenceTex,[switch]$NoFailOnFailure)
$ErrorActionPreference='Stop'
$WpmExe=(Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')
$id=[Guid]::NewGuid().ToString('N'); $root=Join-Path ([IO.Path]::GetTempPath()) ("wpm-config-results-"+$id)
$package="config-results-$id"; $previousData=$env:WPM_DATA_DIR; $env:WPM_DATA_DIR=$root
$started=Get-Date; $results=@()
function Assert-FinalResult([string]$Output,[string]$Expected){$lines=@($Output -split '\r?\n');$matches=@($lines|Where-Object{$_ -eq $Expected});$last=@($lines|Where-Object{-not [string]::IsNullOrWhiteSpace($_)})[-1];if($matches.Count-ne 1-or$last-ne$Expected){throw "Expected exactly one final '$Expected'. $Output"}}
try {
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Global prerelease set ends with a scoped result' -Arguments @('config','set','prerelease','true') -Assert {param($ExitCode,$Output);if($ExitCode-ne 0){throw "Global set failed. $Output"};Assert-FinalResult $Output 'Result: prerelease configured global true'}
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Package prerelease set ends with a scoped result' -Arguments @('config','set','prerelease','false','--package',$package) -Assert {param($ExitCode,$Output);if($ExitCode-ne 0){throw "Package set failed. $Output"};Assert-FinalResult $Output "Result: prerelease configured $package false"}
    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Package prerelease unset ends with a scoped result' -Arguments @('config','unset','prerelease','--package',$package) -Assert {param($ExitCode,$Output);if($ExitCode-ne 0){throw "Package unset failed. $Output"};Assert-FinalResult $Output "Result: prerelease override removed $package"}
} finally {$finished=Get-Date;if($EvidenceTex){Write-WpmTestEvidence -TestCaseId 'TC-0037' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex};if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force};if($null-eq$previousData){Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue}else{$env:WPM_DATA_DIR=$previousData}}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
