param(
    [Parameter(Mandatory = $true)]
    [string]$TestCaseTex,

    [Parameter(Mandatory = $true)]
    [string]$EvidenceTex,

    [Parameter(Mandatory = $true)]
    [string]$OutputTex,

    [string]$PdfLatex
)

$ErrorActionPreference = 'Stop'

$testCasePath = (Resolve-Path -LiteralPath $TestCaseTex).Path
$evidencePath = (Resolve-Path -LiteralPath $EvidenceTex).Path
$outputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputTex)
$outputDir = Split-Path -Parent $outputPath

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$testCaseDir = Split-Path -Parent $testCasePath
$libraryPath = (Join-Path $testCaseDir 'iso_iec_ieee29119-3_2021.tex').Replace('\', '/')
$source = Get-Content -Raw -LiteralPath $testCasePath
$evidence = Get-Content -Raw -LiteralPath $evidencePath

$source = $source -replace '\\input\{iso_iec_ieee29119-3_2021\.tex\}', "\input{$libraryPath}"
$report = $source -replace '\\end\{document\}', "$evidence`r`n\end{document}"

Set-Content -LiteralPath $outputPath -Value $report -Encoding UTF8

if ($PdfLatex) {
    $pdfLatexPath = (Resolve-Path -LiteralPath $PdfLatex).Path
    Push-Location $outputDir
    try {
        & $pdfLatexPath -interaction=nonstopmode -halt-on-error -output-directory $outputDir $outputPath
        if ($LASTEXITCODE -ne 0) {
            throw "pdflatex failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}
