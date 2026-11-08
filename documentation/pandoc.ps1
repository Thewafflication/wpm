# Add project table layout to the pinned WSP documentation builder.
# No param block: forward Pandoc's native options without PowerShell binding.
$ErrorActionPreference = 'Stop'
$pandocBinary = $env:WPM_DOCUMENT_PANDOC
if (-not $pandocBinary) {
    $pandocBinary = (Get-Command pandoc -CommandType Application `
            -ErrorAction Stop | Select-Object -First 1).Source
}
$forwarded = @($args)
$normalizedRoot = Join-Path (Split-Path -Parent $PSScriptRoot) `
    'tmp/pdfs/documentation/normalized'
for ($argumentIndex = 0; $argumentIndex -lt $forwarded.Count; `
        $argumentIndex++) {
    $argument = [string]$forwarded[$argumentIndex]
    if (-not $argument.StartsWith('-') -and
        [IO.Path]::GetExtension($argument) -eq '.tex' -and
        (Test-Path -LiteralPath $argument -PathType Leaf)) {
        New-Item -ItemType Directory -Force -Path $normalizedRoot |
            Out-Null
        $name = '{0:D3}-{1}.md' -f $argumentIndex, `
            [IO.Path]::GetFileNameWithoutExtension($argument)
        $normalized = Join-Path $normalizedRoot $name
        # Parse controlled TeX test specifications as TeX, not literal GFM.
        # Promoting their title gives each manifest input one book chapter.
        & $pandocBinary --from=latex --to=gfm --shift-heading-level-by=-1 `
            "--output=$normalized" $argument
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        $forwarded[$argumentIndex] = $normalized
    }
}
& $pandocBinary "--lua-filter=$PSScriptRoot/table-widths.lua" @forwarded
exit $LASTEXITCODE
