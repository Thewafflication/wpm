# REQ-0011: HTTPS Package Repositories

## Description

WPM shall support named package installation from configured HTTPS repositories.
Only `https://` repository URLs are supported in this release. `file`, SMB,
SCP, and other transports are out of scope.

### Configuration

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

`arch` shall be one of `any`, `x86`, `x64`, or `arm64`. WPM considers only
packages whose architecture is `any` or matches the architecture of the WPM
executable. `url` may be a relative HTTPS path below the repository root or an absolute
HTTPS URL. WPM rejects any other URL scheme. Indexes and package downloads
shall use normal HTTPS certificate validation.

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

An unavailable repository shall not prevent a refresh or resolution from
another repository. A refresh succeeds when at least one repository index is
usable. If no usable index or package can be obtained, WPM shall return a
nonzero exit status without invoking a package install script.

## Verification

Verified by:

- TC-0011 - HTTPS repository configuration and installation
