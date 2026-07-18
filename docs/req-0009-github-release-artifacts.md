# REQ-0009: GitHub Release Artifacts

## Description

When a Git tag is pushed, GitHub Actions shall verify the WPM source, build
Release executables for x86, x64, and ARM64 Windows architectures, and use the
x86 Debug WPM executable that passed the x86 verification job to build a WPM
package for each Release executable. The package job shall download that
immutable verification artifact, including its generated reports and evidence,
and shall not configure, compile, or test a second x86 Debug build.

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
- `wpm-release.public`, the durable Ed25519 public key used to sign official
  WPM release packages
- `install.cmd`, the architecture-selecting latest-release bootstrap installer

The index shall be available through GitHub's stable latest-release asset URL:
`https://github.com/Thewafflication/wpm/releases/latest/download/index.json`.

No release assets shall be published when verification or any architecture
build fails.

The package job shall obtain the durable release private key only from the
protected GitHub `release` environment after any configured approval. It shall
materialize the key only beneath the temporary runner directory, use it to sign
the three packages, and remove it before uploading artifacts. The private key
shall not be uploaded, published, logged, or stored in the repository.

Before upload, the workflow shall add the checked-in durable release public key
to an isolated trust store and run `wpm verify` against each of the three
finished packages. Publication shall not proceed unless all three signatures,
indexes, package metadata files, and payloads validate successfully.

## Rationale

Users need architecture-specific release executables that correspond exactly
to a tagged source revision.

## Verification

Verified by:

- TC-0009 - GitHub Release artifact workflow
- the tag-triggered GitHub Release workflow
