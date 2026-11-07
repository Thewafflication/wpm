# Simple test to verify wpm.exe prints expected version
$wp = "$(PWD)\bin\x64\Release\wpm.exe"
if (-not (Test-Path $wp)) {
    $wp = (Get-ChildItem -Recurse -Filter wpm.exe).FullName
    if (-not $wp) { throw "wpm.exe not found" }
}
# Run wpm --version
$ver = (& $wp --version) -join "`n"
if (-not ($ver -match "Waughtal")) { throw "Version mismatch: $ver" }
Write-Host "Version confirmed: $ver"
