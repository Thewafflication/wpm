# ADR-0003: Package Format and Installation Semantics

Status: Accepted

## Context

WPM is a package distribution, verification, and deployment orchestration
system. WPM is not responsible for enforcing a universal installation layout.

Package maintainers are responsible for installation behavior, installation
location, upgrade behavior, and removal behavior.

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
