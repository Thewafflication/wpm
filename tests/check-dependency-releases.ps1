[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$GitHubToken = $env:GITHUB_TOKEN,
    [string]$SummaryPath = $env:GITHUB_STEP_SUMMARY
)

$ErrorActionPreference = 'Stop'
$dependencies = @(
    @{ Name = 'miniz'; Path = 'third_party/miniz'; Repository = 'richgel999/miniz' },
    @{ Name = 'libsodium'; Path = 'third_party/libsodium'; Repository = 'jedisct1/libsodium' }
)
$dependencyWarnings = [System.Collections.Generic.List[string]]::new()

function Write-DependencyWarning([string]$Message) {
    $dependencyWarnings.Add($Message)
    if ($env:GITHUB_ACTIONS -eq 'true') {
        Write-Output "::warning title=Dependency freshness check::$Message"
    } else {
        Write-Warning $Message
    }
}

function Write-DependencySummary {
    if (-not $SummaryPath) { return }

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add('## Dependency warnings')
    $lines.Add('')
    if ($dependencyWarnings.Count -eq 0) {
        $lines.Add('No dependency freshness warnings were reported.')
    } else {
        foreach ($warning in $dependencyWarnings) {
            $lines.Add("- $($warning.Replace('|', '\|'))")
        }
    }
    $lines.Add('')
    $lines | Out-File -LiteralPath $SummaryPath -Encoding utf8 -Append
}

function ConvertTo-ReleaseVersion([string]$Tag) {
    if ($Tag -notmatch '(?<!\d)(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?') { return $null }
    $revision = if ($Matches[4]) { [int]$Matches[4] } else { 0 }
    return [version]::new([int]$Matches[1], [int]$Matches[2], [int]$Matches[3], $revision)
}

$headers = @{ Accept = 'application/vnd.github+json' }
if ($GitHubToken) { $headers.Authorization = "Bearer $GitHubToken" }

foreach ($dependency in $dependencies) {
    try {
        $path = Join-Path $RepositoryRoot $dependency.Path
        $gitSafety = "safe.directory=$($path.Replace('\', '/'))"
        $pinnedOutput = & git -c $gitSafety -C $path rev-parse HEAD 2>$null
        $pinnedCommit = "$pinnedOutput".Trim()
        if ($LASTEXITCODE -ne 0 -or -not $pinnedCommit) {
            throw "could not read the pinned submodule commit"
        }

        $release = Invoke-RestMethod `
            -Uri "https://api.github.com/repos/$($dependency.Repository)/releases/latest" `
            -Headers $headers
        $latestTag = [string]$release.tag_name
        if (-not $latestTag) { throw 'the latest GitHub release has no tag' }

        $currentOutput = & git -c $gitSafety -C $path describe --tags --exact-match HEAD 2>$null
        $currentTag = "$currentOutput".Trim()
        $currentVersion = ConvertTo-ReleaseVersion $currentTag
        $latestVersion = ConvertTo-ReleaseVersion $latestTag

        if ($currentVersion -and $latestVersion) {
            if ($latestVersion -gt $currentVersion) {
                Write-DependencyWarning "$($dependency.Name) is pinned to $currentTag; GitHub release $latestTag is available."
            } else {
                Write-Host "$($dependency.Name) is pinned to $currentTag; latest GitHub release is $latestTag."
            }
            continue
        }

        # Untagged pins cannot be compared by version. Fall back to ancestry
        # when the latest release tag is available in the submodule checkout.
        & git -c $gitSafety -C $path rev-parse --verify --quiet "refs/tags/$latestTag^{commit}" *> $null
        if ($LASTEXITCODE -ne 0) {
            throw "latest release tag '$latestTag' was not fetched"
        }

        & git -c $gitSafety -C $path merge-base --is-ancestor $pinnedCommit "refs/tags/$latestTag^{commit}"
        $isOlder = $LASTEXITCODE -eq 0
        & git -c $gitSafety -C $path merge-base --is-ancestor "refs/tags/$latestTag^{commit}" $pinnedCommit
        $containsLatest = $LASTEXITCODE -eq 0

        if ($isOlder -and -not $containsLatest) {
            if (-not $currentTag) { $currentTag = $pinnedCommit.Substring(0, 12) }
            Write-DependencyWarning "$($dependency.Name) is pinned to $currentTag; GitHub release $latestTag is available."
        } else {
            Write-Host "$($dependency.Name) contains the latest released tag ($latestTag)."
        }
    } catch {
        Write-DependencyWarning "Could not check $($dependency.Name) release freshness: $($_.Exception.Message)"
    }
}

Write-DependencySummary

# Freshness is advisory and must never fail an otherwise valid build.
exit 0
