# ADR-0005: Repository Architecture and Metadata Format

Status: Accepted

## Context

WPM requires a repository architecture that supports online, offline, local,
and network-based package distribution.

Repositories must support:

- Package discovery
- Package downloads
- Package updates
- Dependency resolution
- Offline operation
- Mirroring
- Secure package distribution

The repository architecture SHALL remain independent from the package trust
model defined in ADR-0001 and ADR-0002.

## Decision

WPM SHALL implement repositories through a provider abstraction layer.

Repository location SHALL NOT affect package trust.

Trust SHALL be established exclusively through:

1. Package signatures
2. Trusted signing keys
3. Manifest verification

Repository transport mechanisms SHALL be interchangeable.

## Repository Providers

WPM SHALL support repository providers.

Initial provider types:

- file
- smb
- https
- scp

Examples:

```text
wpm repo add file:///D:/wpm-repo

wpm repo add smb://nas/packages

wpm repo add https://packages.example.com

wpm repo add scp://buildserver:/srv/wpm
```

Future providers MAY include:

- sftp
- rsync
- git
- cloud object storage

## Repository Structure

Every repository SHALL expose the same logical structure.

Example:

```text
repo/
├── index.json
├── index.sig
├── packages/
│   ├── foo/
│   │   ├── 1.0.0.zip
│   │   └── 1.1.0.zip
│   └── bar/
└── keys/
```

Provider implementations SHALL present this structure consistently regardless
of transport mechanism.

## Repository Metadata

Every repository SHALL contain:

```text
index.json
```

Repository metadata SHALL be human-readable.

JSON SHALL be the canonical metadata format.

Example:

```json
{
  "repository": "Official WPM Repository",
  "version": 1,
  "generated": "2026-06-14T21:00:00Z",
  "packages": [
    {
      "name": "foo",
      "latest": "1.2.0"
    }
  ]
}
```

## Metadata Versioning

Repository metadata SHALL contain a schema version.

Example:

```json
{
  "version": 1
}
```

Changes to repository metadata SHALL increment the schema version.

## Package Metadata

Repository metadata MAY contain package summaries.

Example:

```json
{
  "name": "foo",
  "latest": "1.2.0",
  "description": "Example package",
  "maintainer": "Jordan"
}
```

Package metadata SHALL be advisory.

Package signatures remain authoritative.

## Repository Signatures

Repositories SHALL provide:

```text
index.sig
```

The repository index SHALL be signed.

Repository signatures protect against:

- Metadata tampering
- Package hiding
- Downgrade attacks
- Corrupted mirrors

Repository signatures SHALL NOT replace package signatures.

## Repository Cache

Repository metadata SHALL be cached locally.

Example location:

```text
C:\ProgramData\WPM\repositories\
```

Cached metadata SHALL support offline operations.

## Offline Operations

The following commands SHOULD function without network access after a
successful repository update:

```text
wpm search <query>

wpm info <package>
```

Package installation requires package availability.

## Repository Update

Repositories SHALL refresh metadata through:

```text
wpm update
```

The update process SHALL:

1. Download repository metadata
2. Verify repository signature
3. Update local cache
4. Refresh package catalog

Failed verification SHALL abort the update.

## Mirrors

Repositories MAY define mirrors.

Example:

```ini
[repo]
name=official

mirrors=
  https://repo.example.com
  scp://buildserver:/srv/wpm
  smb://nas/packages
```

Mirror selection MAY occur automatically.

Mirrors SHALL contain identical repository metadata.

## Repository Priority

Repositories SHALL support priority values.

Example:

```ini
priority=100
```

Higher priority repositories SHALL be preferred when identical package versions
exist in multiple repositories.

## Repository Discovery

Repository metadata SHALL support:

- Package search
- Package lookup
- Version discovery
- Dependency discovery

Repository metadata SHALL NOT be considered proof of package authenticity.

## SCP Repositories

WPM SHALL support repositories hosted through SCP.

Example:

```text
wpm repo add scp://buildserver:/srv/wpm
```

SCP repositories provide:

- SSH authentication
- SSH encryption
- Minimal server requirements
- Compatibility with existing infrastructure

OpenSSH-compatible servers SHOULD be supported.

## SMB Repositories

WPM SHALL support repositories hosted on network shares.

Example:

```text
wpm repo add smb://nas/packages
```

SMB repositories support:

- Corporate environments
- Home lab deployments
- Shared package storage

## Local Repositories

WPM SHALL support repositories stored on local storage.

Examples:

```text
wpm repo add D:\wpm-repo

wpm repo add E:\USB\packages
```

Local repositories support:

- Air-gapped systems
- Disaster recovery
- Portable package collections

## Security Considerations

Repository transport SHALL NOT determine trust.

A package downloaded from:

```text
https://repo.example.com
```

and a package loaded from:

```text
E:\USB
```

SHALL undergo identical verification.

Repository compromise SHALL NOT permit installation of malicious packages
without possession of a trusted signing key.

## Future Enhancements

Future versions MAY support:

- Repository snapshots
- Delta synchronization
- Transparency logs
- Package ratings
- Multi-repository federation
- Peer-to-peer repositories

## Consequences

Benefits:

- Transport-independent architecture
- Online and offline support
- SCP and SMB compatibility
- Repository mirroring
- Simple hosting requirements
- Strong separation between transport and trust

Tradeoffs:

- Additional provider implementations
- More repository testing requirements
- Greater metadata management complexity

The design prioritizes flexibility, offline capability, and explicit trust
verification over repository-specific assumptions.
