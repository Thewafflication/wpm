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

function Invoke-WpmTestStep {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WpmExe,

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

function New-WpmManualStep {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    $status = 'Pass'
    $diagnostic = ''
    $output = ''

    try {
        $output = (& $Action 2>&1) -join "`n"
    }
    catch {
        $status = 'Fail'
        $diagnostic = $_.Exception.Message
        $output = "$output`n$diagnostic".Trim()
    }

    [PSCustomObject]@{
        Name = $Name
        Command = 'PowerShell validation'
        ExitCode = if ($status -eq 'Pass') { 0 } else { 1 }
        Status = $status
        Diagnostic = $diagnostic
        Output = $output
    }
}

function Write-WpmTestEvidence {
    param(
        [Parameter(Mandatory = $true)]
        [string]$TestCaseId,

        [Parameter(Mandatory = $true)]
        [string]$WpmExe,

        [Parameter(Mandatory = $true)]
        [datetime]$Started,

        [Parameter(Mandatory = $true)]
        [datetime]$Finished,

        [Parameter(Mandatory = $true)]
        [object[]]$Results,

        [Parameter(Mandatory = $true)]
        [string]$EvidenceTex
    )

    $evidencePath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceTex)
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $evidencePath) | Out-Null
    $overallStatus = if ($Results.Status -contains 'Fail') { 'Fail' } else { 'Pass' }

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add('\section*{Automated Execution Results}')
    $lines.Add('\begin{description}')
    $lines.Add("\item[Test Case Identifier] $(Escape-Latex $TestCaseId)")
    $lines.Add("\item[Execution Timestamp] $(Escape-Latex $Started.ToString('o'))")
    $lines.Add("\item[Completed Timestamp] $(Escape-Latex $Finished.ToString('o'))")
    $lines.Add("\item[Software Under Test] $(Escape-Latex $WpmExe)")
    $lines.Add("\item[Environment] $(Escape-Latex "$([Environment]::OSVersion.VersionString); PowerShell $($PSVersionTable.PSVersion)")")
    $lines.Add("\item[Overall Status] $(Escape-Latex $overallStatus)")
    $lines.Add('\end{description}')

    $stepNumber = 1
    foreach ($result in $Results) {
        $lines.Add("\subsubsection*{Step ${stepNumber}: $(Escape-Latex $result.Name)}")
        $lines.Add('\begin{description}')
        $lines.Add('\item[Command] \mbox{}')
    $lines.Add('\begin{verbatim}')
    $lines.Add((Format-WpmEvidenceText $result.Command))
    $lines.Add('\end{verbatim}')
        $lines.Add("\item[Exit Code] $(Escape-Latex ([string]$result.ExitCode))")
        $lines.Add("\item[Status] $(Escape-Latex $result.Status)")
        if ($result.Diagnostic) {
            $lines.Add("\item[Diagnostic] $(Escape-Latex $result.Diagnostic)")
        }
        $lines.Add('\end{description}')
        $lines.Add('\begin{verbatim}')
        $lines.Add((Format-WpmEvidenceText $result.Output))
        $lines.Add('\end{verbatim}')
        $stepNumber++
    }

    Set-Content -LiteralPath $evidencePath -Value $lines -Encoding UTF8
}

function Format-WpmEvidenceText {
    param(
        [AllowNull()][string]$Value,

        [int]$Width = 72
    )

    if ($null -eq $Value -or $Value.Length -eq 0) {
        return ''
    }

    $wrappedLines = [System.Collections.Generic.List[string]]::new()
    $breakChars = @(' ', '/', '\', '-', '_')
    foreach ($line in ($Value -split "`r?`n")) {
        $remaining = $line
        while ($remaining.Length -gt $Width) {
            $splitAt = -1
            for ($i = [Math]::Min($Width, $remaining.Length - 1); $i -gt 0; $i--) {
                if ($breakChars -contains [string]$remaining[$i]) {
                    $splitAt = $i + 1
                    break
                }
            }

            if ($splitAt -lt 1) {
                $splitAt = $Width
            }

            $wrappedLines.Add($remaining.Substring(0, $splitAt).TrimEnd())
            $remaining = "  " + $remaining.Substring($splitAt).TrimStart()
        }
        $wrappedLines.Add($remaining)
    }

    return ($wrappedLines -join "`n")
}

function Complete-WpmTestRun {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Results,

        [switch]$NoFailOnFailure
    )

    $overallStatus = if ($Results.Status -contains 'Fail') { 'Fail' } else { 'Pass' }
    foreach ($result in $Results) {
        Write-Output "[$($result.Status)] $($result.Command) exited $($result.ExitCode)"
        if ($result.Diagnostic) {
            Write-Output "  $($result.Diagnostic)"
        }
    }

    if ($overallStatus -ne 'Pass' -and -not $NoFailOnFailure) {
        exit 1
    }
}
