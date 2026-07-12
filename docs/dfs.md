# WPM Security Design (DFS)

## Overview

WPM installs software system-wide and is intended to be executed by trusted
administrators. Package installation is a privileged operation.

WPM protects against archive traversal attacks, package tampering, accidental
corruption, and unauthorized package modification.

## Security Goals

WPM SHALL:

- Prevent extraction outside the installation directory.
- Detect package corruption.
- Detect package tampering.
- Verify package authenticity.
- Prevent silent downgrades.
- Maintain installation audit records.

WPM SHALL NOT attempt to sandbox installed software.

Installing a package is considered equivalent to granting the package author
administrative access to the machine.

## Trust Model

Trusted:

- WPM executable
- Operating system
- Trusted signing keys

Untrusted until verified:

- Package archives
- Package indexes
- Downloaded metadata

## Installation Staging and Package Store

WPM SHALL extract each package only to its staging directory:

```text
%ProgramData%\WPM\temp\<archive-name>\
```

Package files SHALL NOT be extracted outside this directory. WPM SHALL verify
the package index before executing package installation logic and shall remove
the staging directory when the attempt completes.

After a successful installation, WPM SHALL retain the original archive beneath:

```text
%ProgramData%\WPM\packages\
```

The package store is an archive store, not a software deployment directory.

## Package Index Verification

Every package SHALL contain:

```text
.wpm/index.csv
```

Format:

```csv
filename,sha256,size
```

Installation SHALL verify:

1. File existence
2. File size
3. SHA-256 hash

Installation SHALL fail if verification fails.

## Package Signatures

Every package MAY contain:

```text
.wpm/signature.sig
```

Ed25519 SHALL be the preferred signing algorithm.

By default:

- Signed packages are verified.
- Unsigned packages are rejected.

Developers may override this behavior:

```text
wpm install package.zip --allow-unsigned
```

Installation SHALL fail if:

- Signature verification fails.
- Package contents differ from signed contents.
- Signing key is not trusted.

## Archive Extraction Security

WPM SHALL reject archive entries containing:

- Relative traversal sequences
- Absolute paths
- UNC paths
- Reserved device names

Examples:

```text
..\Windows\System32
C:\Windows\System32
\\server\share
```

Extraction SHALL abort immediately when such entries are detected.

## Script Execution

Packages may provide:

```text
.wpm/install.cmd
.wpm/remove.cmd
```

Scripts execute with the privileges of WPM.

Before execution WPM SHALL display a warning to the administrator.

## Package Index Security

Package repositories SHALL use HTTPS.

WPM SHALL validate:

- TLS certificates
- Package signatures
- Package-index integrity

HTTPS alone SHALL NOT be considered sufficient proof of authenticity.

## Upgrade Security

Upgrades SHALL verify:

- Package signature
- Package-index integrity
- Version progression

Downgrades SHALL be rejected unless explicitly forced.

## Audit Records

WPM SHALL maintain records containing:

- Package name
- Package version
- Installation timestamp
- Package-index hash
- Signing key identifier

## Future Work

Future versions may support:

- Dependency verification
- Key revocation
- Transparency logs
- Reproducible builds
- Multiple repositories
