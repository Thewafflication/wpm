# REQ-0011: HTTPS Package Repositories

**Content type:** Project requirements

**Status:** Accepted

**Source:** ADR-0002 and ADR-0005 repository model

## Scope

Applies to HTTPS repository configuration, refresh, caching, and package
retrieval on supported Windows targets.

## Requirement

**REQ-0011.001**
WPM shall support named package installation from configured HTTPS repositories.
Only `https://` repository URLs are supported in this release. `file`, SMB,
SCP, and other transports are out of scope.

### Configuration

**REQ-0011.002**
`wpm repo add <https-url> [--priority <integer>]` shall add a repository.
The default priority is 0. The same canonical URL shall be stored once; adding
it again updates its priority. `wpm repo list` shall display configured URLs
and priorities. `wpm repo remove <https-url>` shall remove the matching source
and its cached index.

Configuration is stored in `%ProgramData%\WPM\config\repositories.txt` (or
under `WPM_DATA_DIR`) and repository index caches are stored in
`cache\repositories`. WPM includes
`https://github.com/Thewafflication/wpm/releases/latest/download` as a
built-in priority-0 repository. It provides the published WPM packages and is
used alongside configured repositories; a configured entry for the same URL
may set a different priority.

### Index and package retrieval

**REQ-0011.003**
Each repository shall expose `index.json` at its URL root. Version 1 uses this
schema:

```json
{
  "version": 1,
  "packages": [
    {
      "name": "example",
      "version": "1.2.3",
      "arch": "x64",
      "url": "packages/example-1.2.3.zip"
    }
  ]
}
```

**REQ-0011.004**
`arch` shall be one of `any`, `x86`, `x64`, or `arm64`. WPM considers only
packages whose architecture is `any` or matches the architecture of the WPM
executable. `url` may be a relative HTTPS path below the repository root or an absolute
HTTPS URL. WPM rejects any other URL scheme. Indexes and package downloads
shall use normal HTTPS certificate validation.

**REQ-0011.005**
`wpm repo update` and `wpm update` shall refresh every configured index. After
refreshing, they shall compare cached entries with valid installed package
records and list every eligible newer identity with its architecture, installed
version, and candidate version. They shall explicitly report when all installed
packages are current and shall not download or install package archives.
`wpm install <package-name>` shall resolve a package from cached indexes and
download the selected ZIP to `cache\packages` before using the existing ZIP
installation flow. ZIP path arguments continue to use the existing local
installation flow.

### Resolution, cache, and failures

An index cache is fresh for one hour. Named installation refreshes stale index
caches; it may use a stale cache only when refresh fails and the cache is
present, with a warning. `--offline` prohibits network access and requires a
cached index and cached package ZIP.

WPM selects the highest semantic version available for a package. When the
same version appears in more than one repository, the highest numeric priority
wins; equal priorities are resolved by the order repositories were added.

**REQ-0011.006**
An unavailable repository shall not prevent a refresh or resolution from
another repository. A refresh succeeds when at least one repository index is
usable. If no usable index or package can be obtained, WPM shall return a
nonzero exit status without invoking a package install script.

## Rationale

HTTPS repositories provide broadly deployable package discovery and retrieval
while preserving WPM's separation between transport security and package-signing
trust. Controlled caching supports repeatable offline behavior and recovery from
temporary repository outages.

## Verification

**Method:** Automated test and inspection

**References:** TC-0011 and `tests/tc-0011-*.ps1`

Verified by:

- TC-0011 - HTTPS repository configuration and installation

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`wpm/main.c` and `tests/tc-0011-https-repositories.ps1` currently implement and verify this requirement.
