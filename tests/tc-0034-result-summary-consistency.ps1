param([Parameter(Mandatory=$true)][string]$WpmExe,[string]$EvidenceTex,[switch]$NoFailOnFailure)
$ErrorActionPreference='Stop'
$WpmExe=(Resolve-Path -LiteralPath $WpmExe).Path
. (Join-Path $PSScriptRoot 'wpm-test-lib.ps1')

$id=[Guid]::NewGuid().ToString('N')
$root=Join-Path ([IO.Path]::GetTempPath()) ("wpm-result-summary-"+$id)
$data=Join-Path $root 'data'
$packages=Join-Path $root 'packages'
$pkgName="result-summary-$id"
$source=Join-Path $root 'source'
$previousData=$env:WPM_DATA_DIR
$env:WPM_DATA_DIR=$data
$started=Get-Date; $results=@()

try {
    New-Item -ItemType Directory -Force -Path $packages,(Join-Path $source '.wpm') | Out-Null
    Set-Content -LiteralPath (Join-Path $source '.wpm\package.txt') -Value @(
        "name=$pkgName","version=1.0.0","arch=any","debug=false")
    Set-Content -LiteralPath (Join-Path $source '.wpm\install.cmd') -Value @('@echo off','echo ready')

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Build result-summary package' -Arguments @('build',$source,$packages) -Assert {
        param($ExitCode,$Output) if($ExitCode -ne 0){throw "build failed. $Output"} }
    $archive=(Get-ChildItem -LiteralPath $packages -Filter "$pkgName-*.zip" | Select-Object -First 1).FullName
    if(-not $archive){throw 'Package archive was not created.'}
    Set-Content -NoNewline -LiteralPath (Join-Path $packages 'index.json') -Value (
        '{"version":1,"packages":[{"name":"' + $pkgName +
        '","version":"1.0.0","arch":"any","url":"' +
        (Split-Path -Leaf $archive) + '"}]}')

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Repository add ends with one configured result summary' -Arguments @('repo','add',$packages) -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0){throw "Repository add failed. $Output"}
        $expected="Result: repository configured $packages"
        $lines=@($Output -split '\r?\n')
        $matches=@($lines | Where-Object { $_ -eq $expected })
        $last=@($lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })[-1]
        if($matches.Count -ne 1 -or $last -ne $expected){throw "Repository add did not end with exactly one configured result. $Output"}
    }

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Repository update ends with one operation result summary' -Arguments @('repo','update','--offline') -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0){throw "Repository update failed. $Output"}
        $expected='Result: repositories updated'
        $lines=@($Output -split '\r?\n')
        $matches=@($lines | Where-Object { $_ -eq $expected })
        $last=@($lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })[-1]
        if($matches.Count -ne 1 -or $last -ne $expected){throw "Repository update did not end with exactly one operation result. $Output"}
    }

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Install ends with one identity result summary' -Arguments @('install',$archive,'--allow-unsigned') -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0){throw "Install failed. $Output"}
        $expected="Result: $pkgName any installed"
        $lines=@($Output -split '\r?\n')
        $matches=@($lines | Where-Object { $_ -eq $expected })
        $last=@($lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })[-1]
        if($matches.Count -ne 1 -or $last -ne $expected){throw "Install did not end with exactly one '<name> <arch> installed' result. $Output"}
    }

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Remove ends with one identity result summary' -Arguments @('remove',"$pkgName-any-1.0.0") -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0){throw "Remove failed. $Output"}
        $expected="Result: $pkgName any removed"
        $lines=@($Output -split '\r?\n')
        $matches=@($lines | Where-Object { $_ -eq $expected })
        $last=@($lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })[-1]
        if($matches.Count -ne 1 -or $last -ne $expected){throw "Remove did not end with exactly one '<name> <arch> removed' result. $Output"}
    }

    $results+=Invoke-WpmTestStep -WpmExe $WpmExe -Name 'Repository remove ends with one removed result summary' -Arguments @('repo','remove',$packages) -Assert {
        param($ExitCode,$Output)
        if($ExitCode -ne 0){throw "Repository remove failed. $Output"}
        $expected="Result: repository removed $packages"
        $lines=@($Output -split '\r?\n')
        $matches=@($lines | Where-Object { $_ -eq $expected })
        $last=@($lines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })[-1]
        if($matches.Count -ne 1 -or $last -ne $expected){throw "Repository remove did not end with exactly one removed result. $Output"}
    }
} finally {
    $finished=Get-Date
    if($EvidenceTex){Write-WpmTestEvidence -TestCaseId 'TC-0034' -WpmExe $WpmExe -Started $started -Finished $finished -Results $results -EvidenceTex $EvidenceTex}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
    if($null -eq $previousData){Remove-Item Env:WPM_DATA_DIR -ErrorAction SilentlyContinue}else{$env:WPM_DATA_DIR=$previousData}
}
Complete-WpmTestRun -Results $results -NoFailOnFailure:$NoFailOnFailure
