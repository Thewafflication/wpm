[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$cmakeFile = Join-Path $repositoryRoot 'wpm/CMakeLists.txt'
$sourceRoot = Join-Path $repositoryRoot 'wpm'
$failures = [System.Collections.Generic.List[string]]::new()

$cmake = Get-Content -LiteralPath $cmakeFile -Raw
if ($cmake -notmatch 'C_STANDARD\s+99') {
    $failures.Add('wpm/CMakeLists.txt must set the wpm target C_STANDARD to 99.')
}
if ($cmake -notmatch 'C_STANDARD_REQUIRED\s+ON') {
    $failures.Add('wpm/CMakeLists.txt must require the selected C standard.')
}
if ($cmake -notmatch 'C_EXTENSIONS\s+OFF') {
    $failures.Add('wpm/CMakeLists.txt must disable compiler-specific C extensions.')
}

# C11 language features have no portable C99 spelling. Keep this list narrow so
# identifiers and APIs that merely contain similar words do not produce noise.
$c11Tokens = '(?<![A-Za-z0-9_])(?:_Alignas|_Alignof|_Atomic|_Generic|_Noreturn|_Static_assert|alignas|alignof|atomic_bool|atomic_flag|atomic_int|atomic_uint|noreturn|static_assert|thread_local)(?![A-Za-z0-9_])'
$sourceFiles = Get-ChildItem -LiteralPath $sourceRoot -File |
    Where-Object { $_.Extension -in '.c', '.h' }

foreach ($file in $sourceFiles) {
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $file.FullName) {
        $lineNumber++
        if ($line -match $c11Tokens) {
            $relativePath = [IO.Path]::GetRelativePath($repositoryRoot, $file.FullName)
            $failures.Add("${relativePath}:${lineNumber}: C11 token '$($Matches[0])' is not allowed in C99 source.")
        }
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Host "C99 lint passed for $($sourceFiles.Count) source files."
