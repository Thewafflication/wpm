param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory,

    [Parameter(Mandatory = $true)]
    [int]$ExpectedCount
)

$ErrorActionPreference = 'Stop'
$evidenceFiles = @(Get-ChildItem -LiteralPath $EvidenceDirectory -Filter 'tc-*-execution-evidence.tex' -File)

if ($evidenceFiles.Count -ne $ExpectedCount) {
    throw "Expected $ExpectedCount test evidence files in $EvidenceDirectory, found $($evidenceFiles.Count)."
}

$failed = @()
foreach ($evidenceFile in $evidenceFiles) {
    $statusMatch = Select-String -LiteralPath $evidenceFile.FullName `
        -Pattern '^\\item\[Overall Status\]\s+(Pass|Fail)\s*$' |
        Select-Object -First 1
    if ($null -eq $statusMatch) {
        Write-Output "$($evidenceFile.Name): Overall Status line not found"
        $failed += $evidenceFile.Name
        continue
    }

    $status = $statusMatch.Matches[0].Groups[1].Value
    Write-Output "$($evidenceFile.Name): Overall Status = $status"
    if ($status -ne 'Pass') {
        $failed += $evidenceFile.Name
    }
}

if ($failed.Count -gt 0) {
    throw "Test evidence reported failure or an invalid status: $($failed -join ', ')"
}

Write-Output "Validated $($evidenceFiles.Count) passing test evidence files."
