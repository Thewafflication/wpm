# WPM 2.0 CP-01B Key-Result Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Add final result summaries to key-management mutations

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

Successful key generation, default-key configuration/clear, trust addition,
idempotent trust addition, and trust revocation previously ended with
descriptive prose only. This increment adds one stable final result for each.
Identity-bearing records contain the derived public key ID; they never contain
the private-key path or key material. Default-key operations report state
without repeating their protected path.

Key generation, ACL protection, default selection, trust-store placement,
revocation, idempotence, failure behavior, and cryptography are unchanged.

## Risk and verification

The principal risks are disclosing private material, reporting a different ID
from the mutated trust entry, or emitting duplicate/ambiguous final records.
TC-0036 uses an isolated data root to generate, select, trust twice, revoke,
and clear one key. Every successful command must end with exactly one expected
result, all identity records must use the generated public ID, and generated
output must not contain serialized private-key material. Existing TC-0012
retains the full signing, ACL, trust, revocation, and validation coverage.
REQ-0014.002 remains Planned pending the remaining mutating commands.
