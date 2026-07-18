param(
    [Parameter(Mandatory = $true)]
    [string]$GitHubOutput
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot

function Get-DependencyMetadata {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $gitSafety = "safe.directory=$($Path.Replace('\', '/'))"
    $version = & git -c $gitSafety -C $Path describe --tags --exact-match 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $version) {
        throw "$Name is not pinned to an exact Git tag."
    }
    $commit = & git -c $gitSafety -C $Path rev-parse --short HEAD 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $commit) {
        throw "Could not determine the pinned $Name commit."
    }
    if ($version -notmatch '^[A-Za-z0-9._+-]+$' -or $commit -notmatch '^[0-9A-Fa-f]+$') {
        throw "$Name produced unsafe version metadata."
    }

    [pscustomobject]@{ Name = $Name; Version = $version; Commit = $commit }
}

$dependencies = @(
    Get-DependencyMetadata -Name 'miniz' -Path (Join-Path $repositoryRoot 'third_party\miniz')
    Get-DependencyMetadata -Name 'libsodium' -Path (Join-Path $repositoryRoot 'third_party\libsodium')
)

foreach ($dependency in $dependencies) {
    $prefix = $dependency.Name.ToLowerInvariant()
    "$($prefix)_version=$($dependency.Version)" | Out-File -LiteralPath $GitHubOutput -Append -Encoding utf8
    "$($prefix)_commit=$($dependency.Commit)" | Out-File -LiteralPath $GitHubOutput -Append -Encoding utf8
    Write-Output "$($dependency.Name): $($dependency.Version) ($($dependency.Commit))"
}
