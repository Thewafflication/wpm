# REQ-0009: GitHub Release Artifacts

**Content type:** Project requirements

**Status:** Accepted

**Source:** ADR-0007 release-verification strategy

## Scope

Applies to tagged WPM releases produced by the GitHub Actions release workflow.

## Requirement

**REQ-0009.001**
When a Git tag is pushed, GitHub Actions shall verify the WPM source, build
Release executables for x86, x64, and ARM64 Windows architectures, and use the
x86 Debug WPM executable that passed the x86 verification job to build a WPM
package for each Release executable. The package job shall download that
immutable verification artifact, including its generated reports and evidence,
and shall not configure, compile, or test a second x86 Debug build.

**REQ-0009.002**
The workflow shall use one preliminary metadata job to fetch submodule tags,
resolve each pinned dependency's exact tag and commit, and pass that immutable
metadata to every architecture build. Architecture jobs shall not independently
fetch dependency tags. This ensures all executables report identical dependency
versions while performing tag discovery only once.

**REQ-0009.003**
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

**REQ-0009.004**
The index shall be available through GitHub's stable latest-release asset URL:
`https://github.com/Thewafflication/wpm/releases/latest/download/index.json`.

**REQ-0009.005**
No release assets shall be published when verification or any architecture
build fails. Each Release executable shall start successfully on a runner capable
of executing its target architecture when copied into an otherwise empty
directory, without `wcrt.dll` or another private sidecar runtime. After signed
package validation and before publication, the workflow
shall use the immediately previous published x86, x64, and ARM64 WPM packages
to perform isolated upgrades to their matching candidate packages. Publication
shall not proceed unless all three previous-release upgrades complete and the
installed executables report the candidate version. Candidate packages shall
retain any compatibility sidecar required by the immediately previous release,
even when the candidate executable no longer depends on that sidecar.

**REQ-0009.006**
The package job shall obtain the durable release private key only from the
protected GitHub `release` environment after any configured approval. It shall
materialize the key only beneath the temporary runner directory, use it to sign
the three packages, and remove it before uploading artifacts. The private key
shall not be uploaded, published, logged, or stored in the repository.

**REQ-0009.007**
Before upload, the workflow shall add the checked-in durable release public key
to an isolated trust store and run `wpm verify` against each of the three
finished packages. Publication shall not proceed unless all three signatures,
indexes, package metadata files, and payloads validate successfully.

## Rationale

Users need architecture-specific release executables that correspond exactly
to a tagged source revision.

## Verification

**Method:** Automated test and inspection

**References:** TC-0009 and `tests/tc-0009-*.ps1`

Verified by:

- TC-0009 - GitHub Release artifact workflow
- TC-0013 - Version-aware package installation and upgrades
- the tag-triggered GitHub Release workflow

## Relationships

- **Derived from:** The source and architecture decisions identified above.
- **Depends on:** Applicable package, trust, repository, and lifecycle decisions cited by this requirement.
- **Conflicts with:** None identified.

## Tailoring

None. Applicability changes require the normal WSP adoption and requirement-change process.

## Implementation Record

`.github/workflows/release.yml` and `tests/tc-0009-github-release-artifacts.ps1` currently implement and verify this requirement.
