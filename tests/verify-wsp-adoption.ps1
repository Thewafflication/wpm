[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$recordPath = Join-Path $RepositoryRoot 'docs/wsp-adoption.md'
$record = Get-Content -Raw -LiteralPath $recordPath
$failures = [Collections.Generic.List[string]]::new()

$requirementFiles = @(
    'wsp/README.md',
    'wsp/requirements/requirements-management.md',
    'wsp/documentation/requirements.md',
    'wsp/processes/project-process.md',
    'wsp/testing/test-strategy.md',
    'wsp/security/security-requirements.md',
    'wsp/style/c-style.md',
    'wsp/style/windows-version-resources.md',
    'wsp/security/windows-code-signing-and-defender.md',
    'wsp/tools/requirements.md'
)

$expected = foreach ($relativePath in $requirementFiles) {
    $path = Join-Path $RepositoryRoot $relativePath
    if (-not (Test-Path -LiteralPath $path)) {
        $failures.Add("Pinned WSP requirement source is missing: $relativePath")
        continue
    }
    [regex]::Matches(
        (Get-Content -Raw -LiteralPath $path),
        '(?m)^#{2,3} (WSP-[A-Z]+-\d{4})\b'
    ) | ForEach-Object { $_.Groups[1].Value }
}
$expected = @($expected | Sort-Object -Unique)

$matrixMatch = [regex]::Match(
    $record,
    '(?s)## Requirement Dispositions(.*?)## Tailoring Decisions'
)
if (-not $matrixMatch.Success) {
    $failures.Add('The adoption record does not contain a disposition matrix.')
    $matrix = ''
} else {
    $matrix = $matrixMatch.Groups[1].Value
}

$rows = @(
    [regex]::Matches(
        $matrix,
        '(?m)^\| `(WSP-[A-Z]+-\d{4})` \| ' +
        '(Applicable|Deferred|Not applicable|Tailored) \|'
    ) | ForEach-Object {
        [pscustomobject]@{
            Id = $_.Groups[1].Value
            Disposition = $_.Groups[2].Value
        }
    }
)

foreach ($id in $expected) {
    if ($id -notin $rows.Id) {
        $failures.Add("Missing WSP disposition: $id")
    }
}
foreach ($id in $rows.Id) {
    if ($id -notin $expected) {
        $failures.Add("Unexpected WSP disposition: $id")
    }
}
foreach ($duplicate in $rows | Group-Object Id | Where-Object Count -gt 1) {
    $failures.Add("Duplicate WSP disposition: $($duplicate.Name)")
}

$pinMatch = [regex]::Match(
    $record,
    '(?m)^\*\*Pinned commit:\*\* `([0-9a-f]{40})`\r?$'
)
if (-not $pinMatch.Success) {
    $failures.Add('The adoption record has no full pinned WSP commit.')
} else {
    $gitlink = git -C $RepositoryRoot ls-files --stage wsp
    if ($LASTEXITCODE -ne 0 -or $gitlink -notmatch '^160000 ([0-9a-f]{40}) ') {
        $failures.Add('The repository has no staged or committed wsp gitlink.')
    } elseif ($pinMatch.Groups[1].Value -ne $Matches[1]) {
        $failures.Add(
            "Recorded WSP pin $($pinMatch.Groups[1].Value) does not match " +
            "gitlink $($Matches[1])."
        )
    }
}

$statusMatch = [regex]::Match(
    $record,
    '(?m)^\*\*Status:\*\* (Proposed|Approved)\r?$'
)
if (-not $statusMatch.Success) {
    $failures.Add('The adoption record status must be Proposed or Approved.')
} elseif ($statusMatch.Groups[1].Value -eq 'Approved' -and
    $rows.Disposition -contains 'Deferred') {
    $failures.Add('An Approved adoption record cannot contain Deferred rows.')
}

if ($failures.Count) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}

$counts = $rows | Group-Object Disposition | Sort-Object Name
$summary = $counts | ForEach-Object { "$($_.Name)=$($_.Count)" }
Write-Host (
    "WSP adoption verified for $($rows.Count) requirements " +
    "($($summary -join ', '))."
)
