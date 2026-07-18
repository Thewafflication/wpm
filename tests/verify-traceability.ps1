[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$docs = Join-Path $RepositoryRoot 'docs'
$cmake = Get-Content -Raw -LiteralPath (Join-Path $RepositoryRoot 'wpm/CMakeLists.txt')
$failures = [System.Collections.Generic.List[string]]::new()

$requirements = Get-ChildItem -LiteralPath $docs -Filter 'req-*.md' |
    ForEach-Object { if ($_.BaseName -match '^req-(\d{4})') { $Matches[1] } } |
    Sort-Object -Unique

foreach ($id in $requirements) {
    $testId = "TC-$id"
    $slug = "tc-$id"
    $requirementFile = Get-ChildItem -LiteralPath $docs -Filter "req-$id-*.md"
    if ($requirementFile.Count -ne 1) {
        $failures.Add("REQ-$id must have exactly one requirement document.")
        continue
    }
    $requirement = Get-Content -Raw -LiteralPath $requirementFile.FullName
    $tex = Get-ChildItem -LiteralPath $docs -Filter "$slug-*.tex"
    $script = Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'tests') -Filter "$slug-*.ps1"

    if ($requirement -notmatch [regex]::Escape($testId)) { $failures.Add("REQ-$id does not reference $testId.") }
    if ($tex.Count -ne 1) { $failures.Add("REQ-$id must have exactly one $slug test specification.") }
    if ($script.Count -ne 1) { $failures.Add("REQ-$id must have exactly one $slug automated test.") }
    if ($tex.Count -eq 1) {
        $contents = Get-Content -Raw -LiteralPath $tex.FullName
        if ($contents -notmatch [regex]::Escape("REQ-$id")) { $failures.Add("$($tex.Name) does not reference REQ-$id.") }
    }
    if ($cmake -notmatch [regex]::Escape("wpm_add_test_case($testId")) { $failures.Add("$testId is not registered with CTest.") }
}

if ($failures.Count) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Host "Traceability verified for $($requirements.Count) requirements and test cases."
