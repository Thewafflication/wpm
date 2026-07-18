# ADR-0002: Repository Trust and Key Distribution

Status: Accepted

## Context

WPM repositories distribute package archives and metadata.

Repositories provide availability and discovery, but they are not considered
a root of trust.

A compromised repository must not be able to distribute malicious packages
that are accepted by WPM.

## Decision

WPM SHALL trust signing keys, not repositories.

Repositories act as distribution mechanisms only.

Package acceptance SHALL be based on:

1. Valid package signature
2. Trusted signing key
3. Successful manifest verification

Repository ownership SHALL NOT automatically establish package trust.

## Repository Registration

Repositories may be registered:

wpm repo add https://packages.example.com

Registration only adds a package source.

Registration does not establish trust.

## Repository Trust Bootstrap

Administrators may simultaneously add a repository and trust a signing key:

wpm repo add https://packages.example.com --trust acme-public.pem

This operation performs two independent actions:

1. Add repository
2. Add trusted signing key

The repository itself remains untrusted.

## Multiple Maintainers

A repository may host packages signed by multiple maintainers.

Each package SHALL be validated against its own signing key.

Trust decisions SHALL occur at the package-signing level.

## Repository Metadata

Repository metadata SHALL contain:

- Repository identifier
- Repository URL
- Supported package list
- Package versions
- Package checksums

Repository metadata SHALL NOT be treated as authoritative for trust.

Package signatures remain authoritative.

## Repository Compromise

If a repository is compromised:

- Attackers may remove packages
- Attackers may replace metadata
- Attackers may disrupt availability

Attackers SHALL NOT be able to install malicious packages unless they possess a trusted signing key.

## Key Compromise

If a signing key is compromised:

wpm trust revoke <key-id>

Future installations and upgrades signed by that key SHALL be rejected.

## Offline Verification

WPM SHALL support package verification without repository access.

Example:

wpm verify package.zip

Verification requires:

- Package
- Manifest
- Signature
- Trusted public key

No network access is required.

## Future Transparency Logs

Future versions may implement transparency logs.

Transparency logs may provide:

- Public auditability
- Key history
- Revocation tracking
- Build provenance

Transparency logs SHALL supplement signature verification and SHALL NOT replace it.

## Consequences

Benefits:

- Repository compromise has limited impact
- Trust decisions remain explicit
- Offline verification is possible
- Multiple maintainers can share repositories safely

Tradeoffs:

- More key management
- Less convenience than centralized trust
- Additional onboarding for maintainers

The design favors explicit cryptographic trust over repository-based trust.
