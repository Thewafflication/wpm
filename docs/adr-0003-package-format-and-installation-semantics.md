# ADR-0003: Package Format and Installation Semantics

**Status:** Accepted

**Date:** 2026-06-15

## Context

WPM is a package distribution, verification, and deployment orchestration
system. WPM is not responsible for enforcing a universal installation layout.

Package maintainers are responsible for installation behavior, installation
location, upgrade behavior, and removal behavior.

## Decision Drivers

- WPM must support heterogeneous Windows deployment technologies.
- Packages need a consistent envelope for metadata and verification.
- Scripts need flexibility without weakening pre-execution checks.
- Installed-state records must survive staging cleanup.

## Considered Options

1. Verified ZIP envelope with maintainer-controlled lifecycle scripts.
2. WPM-owned filesystem layout and file ownership.
3. MSI-only packages.
4. Unstructured archives without required metadata.

## Decision

WPM SHALL use a structured ZIP envelope, verify it in temporary staging, and
delegate deployment and removal to package-maintainer scripts.

## Core Principle

WPM manages:

- Package metadata
- Package verification
- Package signatures
- Repository integration
- Package relationships
- Deployment orchestration
- Audit records

Package maintainers manage:

- Installation location
- Installation scripts
- Removal scripts
- Upgrade logic
- Software deployment strategy

## Rationale

The structured envelope supplies stable metadata and verification hooks while
allowing MSI, services, drivers, registry changes, and portable layouts. A
universal WPM-owned layout would exclude or distort common Windows deployments.

## Package Structure

Every package SHALL be distributed as a ZIP archive.

Every package SHALL contain:

.wpm/package.txt
.wpm/index.csv

Optional:

.wpm/signature.sig
.wpm/install.cmd
.wpm/remove.cmd
.wpm/wpmignore.txt

## Package Metadata

Required:

- name
- version

Recommended:

- description
- maintainer
- homepage
- repository

## Installation Behavior

WPM SHALL stage each package at:

C:\ProgramData\WPM\temp\<archive-name>\

WPM SHALL verify the staged package index before invoking `install.cmd`.
After a successful installation, WPM SHALL retain the original ZIP archive
beneath:

C:\ProgramData\WPM\packages\

The staging directory is temporary and SHALL be removed when the installation
attempt completes.

Packages MAY install software anywhere permitted by the operating system.

Examples:

- C:\Program Files\
- C:\Tools\
- Registry
- Windows Services
- MSI packages
- Driver packages

WPM SHALL NOT require a specific software deployment location.

## Deployment Packages

Packages MAY act as deployment containers.

Example:

foo.zip
├── foo.msi
└── .wpm/

install.cmd MAY invoke:

msiexec /i foo.msi

## Package Versions

Multiple versions of a package MAY coexist.

WPM SHALL track installed package metadata independently of software deployment
location.

## Audit Information

WPM SHALL maintain records beneath:

C:\ProgramData\WPM\

Including:

- Package name
- Version
- Installation timestamp
- Verification status
- Signing key information

## Removal

WPM SHALL stage and verify the retained package archive before invoking
`remove.cmd` when present. WPM SHALL delete the retained archive only after
successful package removal and SHALL remove the staging directory after every
removal attempt.

Package maintainers are responsible for removing deployed software.

## Consequences

Benefits:

- Maximum deployment flexibility
- MSI compatibility
- Enterprise software compatibility
- Windows-native workflows

Tradeoffs:

- Reduced filesystem visibility
- More responsibility on package maintainers

WPM prioritizes deployment orchestration over filesystem ownership.

### Follow-up

- Keep format requirements and lifecycle tests synchronized with this decision.
- Define any direct-install transaction model in a superseding ADR.

## References

- REQ-0002 through REQ-0004 and REQ-0006 through REQ-0008
- TC-0002 through TC-0004 and TC-0006 through TC-0008
- ADR-0009: Package Installation Performance
