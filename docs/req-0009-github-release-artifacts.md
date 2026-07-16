# REQ-0009: GitHub Release Artifacts

## Description

When a Git tag is pushed, GitHub Actions shall verify the WPM source, build
Release executables for x86, x64, and ARM64 Windows architectures, and use the
x86 Debug WPM executable to build a WPM package for each Release executable.

The workflow shall fetch tags for every checked-out submodule before generating
version information, so dependency versions identify their exact tags when the
checked-out submodule commit is tagged.

When all required jobs succeed, the workflow shall create or update the GitHub
Release for the pushed tag and attach these assets:

- `wpm-x86-<version>.zip`
- `wpm-x64-<version>.zip`
- `wpm-arm64-<version>.zip`
- `index.json`, containing each published WPM package's name, version,
  architecture, and release-asset URL

The index shall be available through GitHub's stable latest-release asset URL:
`https://github.com/Thewafflication/wpm/releases/latest/download/index.json`.

No release assets shall be published when verification or any architecture
build fails.

## Rationale

Users need architecture-specific release executables that correspond exactly
to a tagged source revision.

## Verification

Verified by:

- TC-0009 - GitHub Release artifact workflow
- the tag-triggered GitHub Release workflow
