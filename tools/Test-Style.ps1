<#
.SYNOPSIS
Checks owned source formatting and configuration syntax.
.DESCRIPTION
Runs the pinned WSP physical scan, PowerShell parser, C/CMake formatters,
Python lint/format checks, and strict JSON/YAML validation. Findings fail
the command. Tool caches, generated output, and vendored code are excluded.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$paths = @(
    'wpm', 'tests', 'tools', 'cmake', '.github', 'documentation',
    'CMakeLists.txt', 'CMakePresets.json', 'third_party/CMakeLists.txt',
    '.pre-commit-config.yaml', '.gitattributes', '.gitignore',
    '.clang-format', '.editorconfig', 'ruff.toml',
    'install.ps1', 'install.cmd', 'setup.cmd', 'remove.cmd'
)

function Invoke-StyleTool {
    param([string]$Name, [string[]]$Arguments)
    $local = Join-Path $root "out/commit-hooks/Scripts/$Name.exe"
    $command = if (Test-Path -LiteralPath $local) { $local } else { $Name }
    & $command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Name style check failed." }
}

Push-Location $root
try {
    & ./wsp/tools/Test-SourceStyle.ps1 -RepositoryRoot $root `
        -SourcePath $paths -ExcludePath @('tools/.cache')
    if (-not $?) { throw 'Physical source style failed.' }
    $files = @($paths | ForEach-Object {
        Get-ChildItem -LiteralPath $_ -Recurse -File
    } | Where-Object { $_.FullName -notlike '*\tools\.cache\*' })
    foreach ($file in $files | Where-Object Extension -EQ '.ps1') {
        $tokens = $null
        $errors = $null
        [void][Management.Automation.Language.Parser]::ParseFile(
            $file.FullName, [ref]$tokens, [ref]$errors)
        if ($errors) { throw "$($file.FullName): $errors" }
    }
    $cFiles = @($files | Where-Object Extension -In '.c', '.h' |
        ForEach-Object FullName)
    Invoke-StyleTool clang-format (@('--dry-run', '--Werror') + $cFiles)
    $cmakeFiles = @($files | Where-Object {
        $_.Extension -eq '.cmake' -or $_.Name -eq 'CMakeLists.txt'
    } | ForEach-Object FullName)
    Invoke-StyleTool gersemi (@('--check') + $cmakeFiles)
    Invoke-StyleTool ruff @('check', 'tests', 'tools')
    Invoke-StyleTool ruff @('format', '--check', 'tests', 'tools')
    Invoke-StyleTool python @('tools/check-configuration.py')
} finally {
    Pop-Location
}
