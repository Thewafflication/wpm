param(
    [Parameter(Mandatory = $true)]
    [string]$WpmExe,

    [string]$EvidenceTex
)

$ErrorActionPreference = 'Stop'
$WpmExe = (Resolve-Path -LiteralPath $WpmExe).Path

function Escape-Latex {
    param([AllowNull()][string]$Value)

    if ($null -eq $Value) {
        return ''
    }

    $escaped = [System.Text.StringBuilder]::new()
    foreach ($char in $Value.ToCharArray()) {
        $null = switch ($char) {
            '\' { $escaped.Append('\textbackslash{}'); break }
            '{' { $escaped.Append('\{'); break }
            '}' { $escaped.Append('\}'); break }
            '&' { $escaped.Append('\&'); break }
            '%' { $escaped.Append('\%'); break }
            '$' { $escaped.Append('\$'); break }
            '#' { $escaped.Append('\#'); break }
            '_' { $escaped.Append('\_'); break }
            '~' { $escaped.Append('\textasciitilde{}'); break }
            '^' { $escaped.Append('\textasciicircum{}'); break }
            default { $escaped.Append($char) }
        }
    }

    return $escaped.ToString()
}

function Invoke-TestStep {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Assert
    )

    $outputLines = & $WpmExe @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output = ($outputLines -join "`n")
    $status = 'Pass'
    $diagnostic = ''

    try {
        & $Assert $exitCode $output
    }
    catch {
        $status = 'Fail'
        $diagnostic = $_.Exception.Message
    }

    [PSCustomObject]@{
        Name = $Name
        Command = "wpm $($Arguments -join ' ')".Trim()
        ExitCode = $exitCode
        Status = $status
        Diagnostic = $diagnostic
        Output = $output
    }
}

$started = Get-Date
$results = @(
    Invoke-TestStep `
        -Name 'Invoke wpm with no command-line arguments' `
        -Arguments @() `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch 'Waughtal Package Manager .* Version ' -or $Output -notmatch 'Usage:') {
                throw 'Expected version and usage information in output.'
            }
        }

    Invoke-TestStep `
        -Name 'Invoke wpm --version --verbose' `
        -Arguments @('--version', '--verbose') `
        -Assert {
            param($ExitCode, $Output)
            if ($ExitCode -ne 0) {
                throw "Expected exit code 0, got $ExitCode."
            }
            if ($Output -notmatch 'Dependencies:' -or $Output -notmatch 'miniz .+commit ' -or $Output -notmatch 'libsodium .+commit ') {
                throw 'Expected verbose dependency version information in output.'
            }
        }
)
$finished = Get-Date
$overallStatus = if ($results.Status -contains 'Fail') { 'Fail' } else { 'Pass' }

if ($EvidenceTex) {
    $evidencePath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceTex)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $evidencePath) | Out-Null

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add('\section*{Automated Execution Results}')
    $lines.Add('\begin{description}')
    $lines.Add("\item[Test Case Identifier] $(Escape-Latex 'TC-0001')")
    $lines.Add("\item[Execution Timestamp] $(Escape-Latex $started.ToString('o'))")
    $lines.Add("\item[Completed Timestamp] $(Escape-Latex $finished.ToString('o'))")
    $lines.Add("\item[Software Under Test] $(Escape-Latex $WpmExe)")
    $lines.Add("\item[Environment] $(Escape-Latex "$([Environment]::OSVersion.VersionString); PowerShell $($PSVersionTable.PSVersion)")")
    $lines.Add("\item[Overall Status] $(Escape-Latex $overallStatus)")
    $lines.Add('\end{description}')

    $stepNumber = 1
    foreach ($result in $results) {
        $lines.Add("\subsubsection*{Step ${stepNumber}: $(Escape-Latex $result.Name)}")
        $lines.Add('\begin{description}')
        $lines.Add("\item[Command] \texttt{$(Escape-Latex $result.Command)}")
        $lines.Add("\item[Exit Code] $(Escape-Latex ([string]$result.ExitCode))")
        $lines.Add("\item[Status] $(Escape-Latex $result.Status)")
        if ($result.Diagnostic) {
            $lines.Add("\item[Diagnostic] $(Escape-Latex $result.Diagnostic)")
        }
        $lines.Add('\end{description}')
        $lines.Add('\begin{verbatim}')
        $lines.Add($result.Output)
        $lines.Add('\end{verbatim}')
        $stepNumber++
    }

    Set-Content -LiteralPath $evidencePath -Value $lines -Encoding UTF8
}

foreach ($result in $results) {
    Write-Output "[$($result.Status)] $($result.Command) exited $($result.ExitCode)"
    if ($result.Diagnostic) {
        Write-Output "  $($result.Diagnostic)"
    }
}

if ($overallStatus -ne 'Pass') {
    exit 1
}
