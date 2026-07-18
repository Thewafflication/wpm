# ADR-0001: Package Trust Model

Status: Proposed

## Context

WPM supports installation of software packages from local files and future
package repositories.

Package signatures provide integrity verification, but signatures alone do not
establish trust. WPM requires a trust model that defines how signing keys are
accepted, trusted, rotated, and revoked.

## Decision

WPM shall use a trust store containing trusted package signing keys.

A package signature is considered valid only when:

1. The package signature verifies successfully.
2. The signing key exists in the local trust store.
3. The signing key has not been revoked.

Signature verification without trust verification is insufficient.

## Trust Store

Trusted keys SHALL be stored locally.

Example:

C:\ProgramData\WPM\trust\

Each trusted key SHALL contain:

- Key identifier
- Public key
- Creation timestamp
- Trust status

Private keys SHALL never be stored by WPM.

## Adding Trust

Administrators may explicitly trust a key:

wpm trust add publickey.pem

Example:

Trusted key added:
Key ID: A1B2C3D4
Owner : Acme Software

Only administrators may modify the trust store.

## Repository Trust

Repositories SHALL NOT automatically establish trust.

Downloading a package from a repository does not imply that the package is
trusted.

Trust is established only through trusted signing keys.

## Unsigned Packages

Unsigned packages are rejected by default.

Administrators may override this behavior:

wpm install package.zip --allow-unsigned

When installing an unsigned package WPM SHALL display a warning.

## Key Rotation

Package maintainers may replace signing keys.

The new key must be explicitly trusted before packages signed with the new key
are accepted.

WPM SHALL NOT automatically trust replacement keys.

Official WPM releases use one durable release key so that users can establish
trust once and authenticate subsequent upgrades. The private key is maintained
outside the repository, supplied only to the protected release environment,
and never published as a workflow artifact. Its public key and key identifier
are published in the repository and release documentation.

Planned rotation uses an overlap period: publish the replacement public key
and identifier through independently controlled project channels, require
administrators to add it explicitly, sign later releases with the replacement,
and revoke the old key after migration. If the old private key is compromised,
administrators must revoke it immediately and explicitly bootstrap trust in the
replacement; possession of an old or new signing key alone cannot silently
change local trust.

## Key Revocation

Administrators may revoke a trusted key:

wpm trust revoke A1B2C3D4

Packages signed by revoked keys SHALL be rejected.

Revocation affects future installations and upgrades.

## Trust on First Use

WPM SHALL NOT implement Trust On First Use (TOFU).

A key must be explicitly trusted before it can be used to validate packages.

This prevents repository compromise from silently establishing trust.

## Audit Records

For every installation WPM SHALL record:

- Package name
- Package version
- Signing key identifier
- Installation timestamp
- Verification result

Audit records SHALL be stored locally.

## Consequences

Benefits:

- Simple trust model
- Easy to audit
- Resistant to repository compromise
- Resistant to man-in-the-middle attacks

Tradeoffs:

- Additional administrator effort
- Manual key management
- Less convenient than automatic trust systems

The design prioritizes security and explicit administrative control over
convenience.
