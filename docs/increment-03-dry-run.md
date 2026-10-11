# I-03: Read-Only Planning and Execution Capabilities

**Content type:** Controlled increment record

**Owner:** WPM maintainers

**Status:** Implemented; completion requires commit gates and native release matrix

## Scope and impact

`--dry-run` supports install, remove, selected upgrade, upgrade-all, and their
self-upgrade plans. Unsupported commands and combinations with `--yes`/`-y`
fail before initialization. Planning shares identity, CRC/index/signature/trust
validation and plan rendering with approved execution. Dry-run returns after
preview and never enters execution or confirmation.

Process-local capabilities are reduced irreversibly for a dry-run invocation.
Writable file opens, directory creation/removal, extraction/build, audit and
source record writes, trust/key/config mutation, package scripts, downloads,
and self-upgrade scheduling require capabilities below command dispatch.
Operational logging is disabled even for configured external destinations and
already initialized sinks. Environment settings cannot grant authority.

Local indexes/payloads are consumed directly. Web indexes/payloads must already
exist in caches; missing data fails without network retrieval or preparation.
This is a deliberate bounded offline preview, not a claim to resolve unseen
network payloads without reading them. Trust additions require explicit prior
verification and `wpm trust add`.

Normal lifecycle planning also reads local sources directly. Uncached web
retrieval requires a distinct cache preparation plan, digest, and approval
before data/cache creation. Deployment approval follows read-only validation.
The execution pass cannot silently add a newly needed cache preparation step.
Logs and data initialization follow deployment approval. Exact selected payload
locators and repository metadata are bound to the plan. Archive fingerprints
are rechecked after confirmation, before initialization, and before scripts.

## Verification allocation

- REQ-0015.004/.005: `dry-run-integration` compares complete disposable directory
  entries, file contents, and modification timestamps before/after successful
  local install/remove/selected/all upgrade and self-upgrade previews.
- The same test starts from an absent data root, exercises external log paths,
  contradictory flags, unsupported build/repo/trust/update commands, missing
  records, unsigned-policy rejection, damaged payloads, and uncached web data.
  Internal completion with preceding color/verbose flags is denied at its
  handoff boundary before its independent log can be opened.
- `readonly-capabilities` calls the production write-open helper directly with
  seven write/update modes, checks retained bytes, and tests write/script/
  handoff/network denial. A lower-level caller cannot restore authority.
- REQ-0015.001/.003/.006: operation integration and TC-0013 retain real lifecycle
  approval, cancellation, stale-state rejection, failure, and handoff coverage.
- Existing signature, repository, logging, script framing, and security tests
  run unchanged in purpose across the native release architecture matrix.

Completion requires lint/build/full commit tests and native x86/x64/ARM64
release verification. Parent REQ-0015 stays Proposed pending composite profile
reconciliation and later recovery allocations. No whole-2.0 verification or
package-script rollback guarantee is implied. Remaining media consumption,
transport policy, recovery, machine schemas, and final quality/readiness work
retain their later roadmap increments.

## Delivery correction

The final source review added the internal completion capability guard after
global-option normalization. Tag `2.0.0-alpha.2` was cancelled before publication;
the corrected I-03 delivery tag is `2.0.0-alpha.2.1`. Native release verification
must use that exact source, including the argument-order no-write regressions.
