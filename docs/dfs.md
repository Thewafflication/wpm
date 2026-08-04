# WPM Design for Security (DFS)

**Content type:** Controlled security design

**Status:** Accepted

**Owner:** WPM maintainers

## 1. Security scope

This DFS covers the WPM executable, package and repository parsers, archive
staging, package-index validation, signing and trust operations, package-script
invocation, command-event presentation, machine serialization, repository
transport and authoring, repository-index signing, operation plans and dry-run,
installed archive and audit storage, recovery records, cleanup, upgrade handoff,
build inputs, and release production.

The intended environment is supported Windows 10 and Windows 11 on x86, x64,
and ARM64. Installation, removal, trust-store mutation, and system-wide upgrade
are privileged administrative activities.

### Protected assets

- **WPM executable and release packages.** Protect authenticity and integrity;
  substitution can execute attacker-controlled package-manager code.
- **Package-signing private keys.** Protect confidentiality and authorized use;
  compromise can forge packages under a trusted identity.
- **Trusted and revoked public-key records.** Protect integrity and authorized
  mutation; compromise can authorize a signer or reactivate a revoked key.
- **Package archives and indexes.** Protect integrity and authenticity and
  enforce bounded processing; failures can enable tampering, path escape, or
  resource exhaustion.
- **Repository configuration and caches.** Protect integrity and provenance;
  compromise can alter source selection or present stale candidates.
- **Repository index-signing keys and authorization.** Protect private-key
  confidentiality and repository-scoped signer-policy integrity; compromise can
  forge or authorize misleading catalogs without authorizing package content.
- **Authored repositories and replacement files.** Protect containment,
  integrity, deterministic identity, and availability; authoring crosses a
  privileged destructive filesystem boundary.
- **Operation plans and mutation capability.** Protect integrity and binding to
  confirmation; substitution can authorize a different package, source, or
  mutation set, and accidental capability use can violate dry-run purity.
- **Command and machine output.** Protect integrity, parseability, attribution,
  and confidentiality; injection or redaction failure can mislead users, break
  automation, or disclose protected data.
- **Installed-package archive records.** Protect integrity and availability;
  corruption can cause incorrect removal, upgrade, or installed-state decisions.
- **Audit, failure, and recovery records.** Protect confidentiality, integrity,
  availability, schema identity, and linkage so attribution, diagnosis, retry,
  cleanup classification, and retained evidence remain usable.
- **Cleanup classifications and allowed roots.** Protect integrity and scope;
  an incorrect or raced classification can delete active state or sole recovery
  evidence.
- **Credentials and repository or proxy secrets.** Protect confidentiality to
  prevent unauthorized external access.

### Assumptions

- The operating system, administrator account, filesystem security, TLS stack,
  GitHub release service, and reviewed cryptographic dependencies behave as
  documented.
- Administrators establish signing-key trust through an independent and
  appropriate channel before relying on a package publisher.
- A trusted package script is intentionally granted the privileges of WPM.
- Filesystem atomic replacement, volume identity, reparse-point, and sharing
  behavior are used only where the supported Windows/filesystem combination
  provides the verified guarantees; otherwise the operation fails closed.

### Explicit non-goals

- WPM does not sandbox, prove safe, or constrain the post-authorization behavior
  of package-provided scripts.
- WPM does not make a repository, HTTPS endpoint, archive filename, or successful
  parse a trust anchor.
- WPM does not provide universal rollback for arbitrary package scripts.
- WPM does not provide confidentiality, endpoint authentication, or freshness
  for an explicitly enabled HTTP transport.
- WPM does not discover or remount moved removable media, manage SMB
  credentials, create optical filesystems, or make media ownership a trust
  anchor.
- WPM does not make a repository-index signer a package-signing authority and
  does not obtain repository-signing trust from repository content.
- Dry run does not execute package scripts or predict their undocumented effects.
- Recovery records are not executable journals, and cleanup is not a general
  product uninstaller or evidence-retention bypass.
- Human prose is not a stable machine interface; package-script text is not a
  trusted WPM result event.
- WPM adoption of WSP does not claim security certification.

## 2. Security goals and derived requirements

- **Keep archive writes inside controlled staging:** REQ-0004 and REQ-0012;
  verified by TC-0004, TC-0012, and source inspection.
- **Detect missing, added, modified, or truncated package content:** REQ-0005
  and REQ-0012; verified by TC-0005 and TC-0012.
- **Execute packages only after authorization and validation:** REQ-0004 and
  REQ-0012; verified by TC-0004 and TC-0012.
- **Separate repository transport from package trust:** REQ-0011 and REQ-0012;
  verified by TC-0011 and TC-0012.
- **Keep local, removable, SMB, HTTPS, and opted-in HTTP access behind one
  read-only validation boundary:** REQ-0016; planned verification by TC-0016.
- **Contain repository authoring and separately authorize index signers:**
  REQ-0017; planned verification by TC-0017.
- **Prevent presentation injection, machine-output corruption, and protected
  data leakage:** REQ-0014 and REQ-0018; planned verification by TC-0014 and
  TC-0018.
- **Bind confirmation to an exact plan and enforce mutation-free dry run:**
  REQ-0015; planned verification by TC-0015.
- **Make recovery inspectable without blind replay and cleanup root-contained:**
  REQ-0019; planned verification by TC-0019.
- **Prevent silent architecture changes, downgrade, or invalid upgrade state:**
  REQ-0013; verified by TC-0013.
- **Preserve installation, failure, signer, and upgrade evidence:** REQ-0012
  and REQ-0013; verified by TC-0012 and TC-0013.
- **Protect release identity and signing inputs:** REQ-0009 and REQ-0012;
  verified by TC-0009, TC-0012, and workflow inspection.

These project requirements derive the applicable `WSP-SEC-0001` through
`WSP-SEC-0014` obligations into the WPM baseline. Requirement changes follow
the controlled traceability and impact-analysis process.

## 3. Trust model

### Trusted components and data

- the exact WPM executable selected by the administrator;
- the supported Windows security and filesystem primitives used by WPM;
- pinned and recorded build dependencies;
- public keys explicitly present in the active trust store; and
- repository-index public keys explicitly pinned for that repository and role;
- protected release infrastructure and signing identity within their defined
  operating procedures.

### Partially trusted components

- HTTPS and GitHub provide authenticated transport for their endpoint but do
  not authorize a package signer;
- SMB authentication and filesystem permissions authorize access to a source,
  not repository-index or package content;
- repository indexes and caches may guide discovery only after applicable
  validation;
- versioned recovery records may guide inspection only after schema, path,
  artifact, current-state, and trust revalidation;
- installed archive records describe local state but require full validation
  before reused content executes; and
- package metadata is trusted only for the operation and validation level that
  consumed it.

### Untrusted inputs

- package archives, indexes, signatures, metadata, and scripts before validation;
- repository responses, redirects, cache contents, filenames, URLs, local and
  removable media contents, UNC shares, and authored repository trees;
- command-line arguments, environment overrides, configuration, and paths;
- package names, versions, architectures, script output, event fields, and
  recovery-record content; and
- prior or externally modified WPM data-directory contents.

### Trust boundaries and privileged operations

- **Locator to repository reader:** absolute filesystem, removable-media, UNC,
  HTTPS, and opted-in HTTP locators cross only after typed parsing, policy,
  source-identity capture, bounded reads, path resolution, and redaction.
- **Repository source to cache:** source data crosses only after complete
  bounds, format, signature/policy, and identity validation; earlier usable
  cache is retained until atomic commit. HTTP and SMB access do not cross a
  trust boundary merely because transport succeeds.
- **Untrusted archive to authoring root:** a package crosses only after archive,
  metadata, index, signature/trust, destination-containment, conflict, and
  available-space checks followed by atomic file commit.
- **Private authoring key to index signature:** a private key crosses into an
  ephemeral signing operation only after explicit selection and repository-
  scoped signer authorization; only the public key ID and detached signature
  leave the boundary.
- **Command/query result to presentation:** structured fields cross to human,
  JSON, and log renderers only after sensitivity classification, redaction,
  encoding, bounds, and package-script attribution.
- **Resolved plan to mutation:** confirmation authorizes a canonical plan
  digest; only a mutation-capable executor may cross after identity recheck.
  Dry run receives no mutation capability.
- **Archive to staging filesystem:** build, install, verify, remove, and upgrade
  cross only after canonical containment, type/size checks, and controlled
  extraction.
- **Staged content to execution:** install, remove, and upgrade scripts cross
  only after index validation, signature/trust policy, metadata validation, and
  an explicit operation.
- **User context to machine trust:** `wpm trust add/revoke` crosses only after
  administrative authorization and exact key-identity validation.
- **CI source to published release:** the release workflow crosses only after
  pinned inputs, matrix gates, protected signing input, and artifact verification.
- **Current process to upgrade handoff:** self-upgrade crosses only after
  candidate validation, cached handoff identity, audit logging, and a recovery
  record.
- **Recovery record to retry:** a record crosses only as non-executable identity
  input after schema, containment, retained artifact, trust, and current-state
  revalidation and construction of a newly confirmed plan.
- **Cleanup inventory to deletion:** a classified object crosses only after
  allow-listed-root containment, active/recovery/evidence exclusion, reparse and
  identity checks, and an immediate pre-delete recheck.

Network location, repository ownership, successful TLS, file ownership, or
successful parsing alone never crosses a package-trust boundary.

## 4. Threat analysis and control traceability

Each threat record identifies its scenario, controls, verification, and residual
risk.

- **WPM-THR-001 - Archive path escape.** Traversal, absolute, UNC,
  reserved-device, separator, or canonicalization tricks may write outside
  staging. Reject unsafe names, require canonical containment, and use a bounded
  staging root. Verify with TC-0004, TC-0012, and archive-source inspection.
  Residual risk: OS/filesystem differences may expose untested aliases.
- **WPM-THR-002 - Package tampering.** An attacker may add, remove, change, or
  truncate content. Use BLAKE2b indexes, size/completeness checks, and an Ed25519
  signature over the index. Verify with TC-0005 and TC-0012. Residual risk: a
  trusted signer can intentionally publish harmful content.
- **WPM-THR-003 - Repository compromise.** A repository or distribution path may
  substitute metadata or packages. Use transport policy, repository-index
  signature policy, package signatures, local role-specific signer trust,
  immutable source identity, atomic cache commit, and offline verification.
  Verify with TC-0011 and TC-0012 and plan with TC-0016 and TC-0017. Residual
  risk: availability, hiding, replay, and stale-cache attacks remain possible
  within version and signer policy.
- **WPM-THR-004 - Signing-key compromise.** A stolen private key may sign
  malicious packages. Keep keys outside the repository, protect the release
  and authoring secrets, separate repository-index and package roles, support
  revocation, and use durable key IDs. Verify with TC-0009 and TC-0012 and plan
  with TC-0017 plus release inspection. Residual risk: undiscovered compromise
  and previously trusted packages or catalogs remain risks.
- **WPM-THR-005 - Trust-store mutation.** An unprivileged actor may add or
  reactivate a key. Require administrative authorization, separate active and
  revoked records, and validate exact key IDs. Verify with TC-0012. Residual
  risk: a compromised administrator or OS can alter local trust.
- **WPM-THR-006 - Malformed or excessive input.** Crafted archives, metadata,
  URLs, locators, indexes, events, recovery records, or arguments may exhaust
  resources or confuse parsers. Enforce length/range/format/encoding checks,
  bounded reads, controlled temporary roots, and fail-closed parsing. Verify
  with TC-0002 through TC-0006 and TC-0010 through TC-0013 and plan with
  TC-0014 through TC-0019. Residual risk: large valid packages and result sets
  still consume resources.
- **WPM-THR-007 - Privileged script abuse.** An authorized script may modify
  arbitrary system state. Validate before execution, require an explicit
  operation/warning, and record signer attribution and audit data. Verify with
  TC-0004, TC-0012, and TC-0013. Residual risk: script behavior is intentionally
  outside WPM's sandbox boundary.
- **WPM-THR-008 - Downgrade or candidate confusion.** Architecture, prerelease,
  priority, or SemVer ambiguity may select unintended content. Enforce exact
  identities, SemVer, architecture filtering, prerelease policy, and downgrade
  rules. Verify with TC-0011 and TC-0013. Residual risk: repository freshness
  and operator override affect selection.
- **WPM-THR-009 - Interrupted lifecycle operation.** Script, authoring, cache,
  record, cleanup, or handoff failure may leave deployed software and WPM state
  inconsistent. Use immutable plans, prevalidation, per-file atomic commit,
  retained archives, failure audits, versioned recovery state, and conservative
  continuation rules. Verify with TC-0013 and plan with TC-0015, TC-0017, and
  TC-0019 plus prior-release upgrade tests. Residual risk: arbitrary scripts and
  multi-file filesystem limits prevent universal atomicity or rollback.
- **WPM-THR-010 - Secret disclosure.** Keys or credentials may enter source,
  locators, plans, logs, JSON, recovery records, arguments, artifacts, or
  diagnostics. Keep release and authoring keys uncommitted, reject credential-
  bearing locators, classify/redact event fields centrally, clear key material,
  protect CI secrets, and inspect every output destination. Verify with TC-0009
  and TC-0012 and plan with TC-0014 through TC-0019 plus artifact inspection.
  Residual risk: endpoint compromise can expose secrets during authorized use.
- **WPM-THR-011 - Build or dependency substitution.** Modified tools,
  submodules, or dependencies may change release behavior. Pin submodules,
  record versions, control setup, run the architecture matrix, and verify the
  release. Verify with TC-0009 and workflow/dependency inspection. Residual risk:
  external package distribution remains a supply-chain dependency.
- **WPM-THR-012 - Evidence deletion or forgery.** Evidence may be altered,
  omitted, overwritten, or misclassified as cleanup. Use fail-closed initial
  recovery/audit writes, atomic updates, retention-aware cleanup exclusions,
  retained failures, immutable CI/release association, and checksums/signatures.
  Verify with TC-0012 and TC-0013 and plan with TC-0019 plus release-record
  inspection. Residual risk: a local administrator can alter local records.
- **WPM-THR-013 - HTTP use or redirect downgrade.** An on-path attacker may
  observe, replay, redirect, truncate, or replace HTTP content, or an HTTPS
  source may be downgraded. Require repository-scoped opt-in, conspicuous use
  warnings, redirect reparsing and origin/scheme policy, content bounds,
  unchanged index/package validation, and atomic cache preservation. Plan
  verification with TC-0016. Residual risk: opted-in HTTP provides no
  confidentiality, endpoint authentication, or independent freshness.
- **WPM-THR-014 - Removable or shared-media substitution.** Media may disappear,
  change drive letter, be replaced, change between plan and use, or return
  partial data. Capture and recheck volume/source and object identity, never
  search for a replacement, validate every read, and retain prior usable cache.
  Plan verification with TC-0016 and TC-0017 plus real-media demonstration.
  Residual risk: device and filesystem identity strength varies by environment.
- **WPM-THR-015 - Repository authoring escape or overwrite race.** Crafted
  archive paths, reparse points, links, or concurrent replacement may write
  outside the repository or overwrite unrelated content. Canonicalize one
  allow-listed root, reject unsafe types, use exclusive sibling temporary files,
  recheck destination identity, and atomically commit. Plan verification with
  TC-0017. Residual risk: filesystem/filter-driver semantics can differ from
  deterministic fixtures.
- **WPM-THR-016 - Repository signer-role confusion or downgrade.** Repository
  content may introduce a key, a valid package signer may be mistaken for an
  index signer, or `index.sig` may be removed. Keep repository-scoped independent
  signer pins, refuse repository-provided bootstrap, bind signature to exact
  index bytes, and make required-signature policy non-downgradable. Plan
  verification with TC-0017. Residual risk: an authorized index signer can hide,
  replay, or prioritize otherwise valid packages.
- **WPM-THR-017 - Output injection or machine-schema corruption.** Untrusted
  names, script text, control bytes, invalid encoding, or partial serialization
  may impersonate WPM status or corrupt automation. Use typed attributed events,
  centralized redaction, bounded encoding-aware serializers, semantic labels,
  one atomic JSON envelope, and explicit serializer failure. Plan verification
  with TC-0014 and TC-0018. Residual risk: terminals and downstream consumers
  may interpret valid Unicode or displayed paths differently.
- **WPM-THR-018 - Dry-run mutation or plan substitution.** A hidden helper may
  write state during dry run, or inputs may change after confirmation. Enforce
  read-only capabilities below dispatch, prohibit persistent logs and scripts,
  bind confirmation to a canonical plan digest, and recheck identities before
  mutation. Plan verification with TC-0015 and TC-0017. Residual risk: read-only
  OS and antivirus activity is outside WPM and timing-dependent source changes
  remain denial-of-service vectors.
- **WPM-THR-019 - Unsafe recovery replay.** A stale, forged, malformed, or
  secret-bearing recovery record may redirect writes or replay a script under
  changed trust/state. Use a versioned bounded schema, protected atomic storage,
  no executable arguments, allow-listed paths, full retained-input/current-state
  revalidation, new planning, and new confirmation. Plan verification with
  TC-0019. Residual risk: local administrators can forge records and external
  script effects may be unknowable.
- **WPM-THR-020 - Cleanup escape or active-state deletion.** Malicious names,
  reparse points, stale classification, races, or broad roots may delete active
  state or sole recovery evidence. Inventory before mutation, classify unknown
  state as retained, use exact allow-listed descendants, exclude active and
  retained evidence, recheck each identity, and fail individual races closed.
  Plan verification with TC-0019 and destructive-boundary review. Residual risk:
  locked files can remain and local administrators may delete data outside WPM.

## 5. Security control design

### 5.1 Archive and resource control

WPM extracts only into an operation-specific staging directory. It validates
archive entry structure and canonical containment before a path affects the
filesystem. Package metadata reads use explicit bounds, including the 1 MiB
metadata-only inspection limit. Validation failure aborts before package-script
execution and produces a nonzero result.

Temporary and cached content belongs beneath controlled WPM data roots. Cleanup
after success and failure is plan-driven. A cleanup inventory treats unknown,
active, recoverable, and evidence-retained objects as non-removable, rechecks
identity immediately before deletion, and rejects escaping roots, device paths,
reparse traversal, unsafe file types, and broadened deletion after a race.

### 5.2 Content integrity and completeness

`.wpm/index.csv` records each indexed path, size, BLAKE2b digest, and algorithm.
Installation and verification reject malformed records, missing or additional
files, size differences, digest differences, duplicate ambiguity, and unsafe
paths. The index is loaded and checked independently from archive filenames.

### 5.3 Signing, key lifecycle, and authorization

WPM uses Ed25519 package-index signatures through the reviewed libsodium
implementation. A signature is accepted only when its structure and algorithm
are supported, the declared key identity matches the public key, the signature
verifies, and local trust policy authorizes the key.

Private keys are never placed in the WPM trust store. Generated private-key
files receive restrictive permissions where supported, secret buffers are
cleared after use, and release private-key material is supplied only to the
protected release job. Revocation is persistent and prevents silent re-adding
of the same key.

Repository-index signer authorization is a separate, repository-scoped role.
An administrator pins the public key through an independent channel. Repository
content, TLS/SMB authentication, HTTP opt-in, or package-signing authorization
cannot establish this role. Index signing reads an explicitly selected private
key only during the authorized operation, emits only its public key ID, clears
secret buffers, and never copies the key into repository content or evidence.

Unsigned installation is denied by default. The `--allow-unsigned` exception
requires an explicit request and an authorized context, emits a warning, and is
recorded as unsigned. It is a deliberate administrative bypass, not trust.

### 5.4 Repository and cache security

Typed locators place absolute/drive-root/removable paths, UNC shares, HTTPS, and
repository-scoped opted-in HTTP behind one read-only source interface. Windows
path grammar is resolved before URI grammar; ambiguous, relative, device,
credential-bearing, and unsupported locators are rejected. Filesystem and UNC
sources are never modified, and WPM never searches for media after a drive-
letter or identity change.

HTTPS uses the Windows TLS validation stack. UNC uses the caller's Windows
authorization. HTTP requires persisted repository-scoped opt-in, warning, and
audit identity; redirects are reparsed and cannot downgrade or change origin
silently. These transports provide different access protection but never
authorize content.

Repository priority, architecture, version, cache, and offline rules remain
deterministic. Index/package paths remain within the logical root. New cache
content becomes authoritative only after complete bounded reads, source identity
recheck, format and signature policy, package validation, and atomic commit;
failure retains the earlier usable cache.

Repository authoring uses a separate local-only writer. It validates archives
before an exclusive temporary copy, rechecks destination/root identity, and
commits files atomically. Deterministic version-1 indexes contain only portable
relative paths. Read-only verification writes no repository, cache, trust,
audit, or recovery state.

### 5.5 Least privilege and package scripts

Read-only inspection and verification do not intentionally modify package,
trust, or audit state. Machine trust mutation and privileged unsigned-package
exceptions require authorization. WPM performs validation before invoking
install or removal scripts and attributes resulting records to package and
signer identity.

Resolution creates an immutable plan and digest before confirmation. Execution
rechecks plan identities and obtains mutation capability only after authorization.
Dry run receives read-only capabilities below dispatch and cannot create
staging, cache, logs, audit/recovery, configuration, trust, repository, retained
artifact, process, or handoff state or invoke a package script.

WPM cannot reduce the privileges of an administrator-approved script without a
separate sandbox and compatibility model. This accepted boundary is visible in
warnings, documentation, and residual-risk decisions.

### 5.6 Logging and protected data

Typed command events are classified and redacted before delivery to human,
machine, and log renderers. Renderers bound and escape untrusted fields;
terminal control is human-renderer-only; package-script streams remain opaque,
bounded, and attributed. Handled machine results use one versioned UTF-8 JSON
envelope with deterministic ordering and no prompts, styling, progress, or prose.

Installation and upgrade records include package identity, version transition
where applicable, plan digest, verification outcome, signing-key identity or
unsigned state, timestamp, and failure stage or script exit status when
relevant. Logs and JSON shall not contain private-key material, credentials,
URL user information, tokens, or avoidable protected data.

Local audit files are diagnostic evidence, not tamper-proof security logs. CI
and release evidence uses controlled artifact association and release identity
for stronger integrity.

### 5.7 Secure failure and recovery

Parsing, containment, signature, trust, index, metadata, architecture, and
candidate-selection failures stop before execution. WPM does not silently grant
trust or discard a required failure result. Upgrade failures retain the prior
archive records and write recovery-relevant audit information.

Arbitrary package scripts may partially modify the system before failing.
Recovery is package-specific unless a controlled self-upgrade handoff or
package-defined rollback behavior applies.

Before its first durable mutation, an operation writes an atomic bounded
`wpm.recovery.v1` record containing non-secret identity, phase, plan digest,
validation/signer state, retained artifacts and digests, external-state
uncertainty, and safe next actions. Failure to create it blocks mutation. The
last valid record is retained if a later update fails.

Inspection treats the record as untrusted read-only data. Retry never executes
stored commands or paths: it revalidates schema, containment, retained content,
current state, source, signatures, and trust, then builds and confirms a new
plan. Unsupported/legacy state is retained and may yield inspect-only guidance.
Cleanup cannot delete the only eligible recovery input or evidence within its
retention period.

### 5.8 Build and release integrity

Release workflows check out recursive pinned submodules, provision recorded
tool and dependency versions, test x86/x64/ARM64 configurations, protect signing
material from artifacts and logs, verify signed output, and gate publication on
required verification and previous-release upgrade results.

Known dependency vulnerabilities and material toolchain changes are assessed as
part of release review and WSP baseline upgrades.

## 6. Residual-risk decisions

- **Trusted scripts have administrator-equivalent effect - Accepted.** Explicit
  trust, validation, warning, attribution, and audit compensate; sandboxing is a
  non-goal. Review on a new sandbox requirement or material abuse case.
- **Local administrators can alter trust and audit files - Accepted.** The
  administrator is the local security boundary; release evidence is separately
  protected. Review for multi-user trust or remote administration.
- **No universal rollback for arbitrary scripts - Accepted.** Prevalidation,
  retained archives, failure audits, and package-specific recovery compensate.
  Review when direct-install or transaction metadata is designed.
- **Repositories can withhold or replay content - Accepted with controls.**
  Repository/index and package signatures prevent unauthorized catalog/content
  after their respective signer policy; SemVer and policy constrain selection.
  An authorized index signer can still hide or replay entries. Review for
  snapshots or transparency logs.
- **Opted-in HTTP has no confidentiality, endpoint authentication, or independent
  freshness - Accepted for explicitly scoped repositories.** Repository-scoped
  opt-in, warnings, redirect restrictions, signatures, bounded reads, and atomic
  cache validation compensate for integrity but not observation or denial of
  service. Review on a stronger transport availability requirement or incident.
- **Removable, optical, and SMB identity/availability varies - Accepted with
  fail-closed behavior.** Source/object rechecks and no automatic relocation
  prevent silent substitution; real-environment release evidence covers declared
  journeys. Review on a recurring alias, filesystem, or reconnect defect.
- **Repository-index signers can publish misleading but package-valid catalogs -
  Accepted with role separation.** Independent repository-scoped pins,
  package-signature validation, and explicit legacy diagnostics compensate.
  Review for snapshot, expiry, threshold-signing, or transparency requirements.
- **Filesystem replacement and cleanup cannot be universally race-free - Accepted
  with conservative scope.** Same-volume atomic files, allow-listed roots,
  reparse rejection, identity rechecks, and retained unknown/locked state
  compensate. Review on a destructive escape or unsupported filesystem claim.
- **Recovery records and local output are administrator-modifiable - Accepted.**
  Records are non-executable, bounded, revalidated, and never sufficient for
  retry authorization; central redaction and protected file permissions reduce
  disclosure. Review for remote/multi-user administration or tamper-evident
  local audit requirements.
- **CI cannot cover every Windows environment - Accepted.** The architecture
  matrix, support policy, fault injection, and field response compensate. Review
  for a new support claim or recurring compatibility defect.

No residual-risk decision authorizes bypassing a required release gate without
the deviation record defined by `docs/ts-0001-test-strategy.md`.

## 7. Security verification

Security verification combines negative automated tests, fault injection,
source and workflow inspection, dependency review, traceability validation, and
architecture-specific release execution. Applicable security requirements and
accepted threats map to the evidence above.

TC-0023 statically verifies that the architecture/DFS baseline contains the
required decisions and bidirectional references; it is not runtime security
evidence. Planned TC-0014 through TC-0019 must verify output injection and
redaction, dry-run state snapshots, every transport's unchanged signature/trust
checks, source/media faults, authoring containment and signer roles, deterministic
machine schemas, recovery tampering/revalidation, and cleanup deletion bounds.
Native SMB, removable/optical media, protected signing, and x86/x64/ARM64
recovery evidence remain required where deterministic fixtures are insufficient.

Failures are recorded through the normal defect process with affected versions,
severity, reproduction or indicators, corrective change, regression coverage,
and residual-risk approval where a full correction is not made.

## 8. Vulnerability response

Report suspected vulnerabilities privately through GitHub's private
vulnerability-reporting or security-advisory channel for the WPM repository. If
that channel is unavailable, open a minimal public issue requesting private
coordination without including exploit details, secrets, or affected-user data.

Maintainers shall:

1. acknowledge and preserve the report privately;
2. assess credibility, affected versions, exploitability, impact, and whether a
   signing key or release channel may be compromised;
3. classify urgency as Critical, High, Moderate, or Low using impact and
   likelihood rather than an unsupported certification claim;
4. contain active compromise, including release suspension or key revocation;
5. implement and review a correction or explicitly accept residual risk;
6. verify the correction with regression and release-matrix evidence;
7. publish a supported-version security release and coordinated advisory when
   disclosure is safe; and
8. record affected versions, resolution, verification, disclosure, and any key
   or trust migration instructions.

Critical or active exploitation may use an emergency change, but package trust,
artifact verification, and release-identity gates remain mandatory. The support
scope and absence of guaranteed response times are defined in
`docs/support-policy.md`.

## 9. Review and change control

Review this DFS when a security-relevant interface, parser, trust boundary,
privilege, dependency, asset, threat, signing process, repository behavior,
update path, support claim, or control materially changes. Each review checks
requirements, ADRs, implementation, tests, evidence, and residual risk together.

## 10. References

- `docs/adr-0001-package-trust-model-full.md`
- `docs/adr-0002-repository-trust-and-key-distribution-full.md`
- `docs/adr-0003-package-format-and-installation-semantics.md`
- `docs/adr-0008-metadata-only-installed-package-inspection.md`
- `docs/adr-0009-package-installation-performance.md`
- `docs/adr-0010-command-events-and-machine-readable-output.md`
- `docs/adr-0011-transport-neutral-repository-access.md`
- `docs/adr-0012-repository-authoring-and-index-signing.md`
- `docs/adr-0013-operation-plans-dry-run-and-recovery.md`
- `docs/req-0004-package-installation.md`
- `docs/req-0005-package-index-signature-verification.md`
- `docs/req-0009-github-release-artifacts.md`
- `docs/req-0011-https-repositories.md`
- `docs/req-0012-package-signing-and-validation.md`
- `docs/req-0013-version-aware-upgrades.md`
- `docs/req-0014-command-output-and-help.md`
- `docs/req-0015-safer-package-changes.md`
- `docs/req-0016-repository-transports.md`
- `docs/req-0017-repository-authoring.md`
- `docs/req-0018-discoverability-and-diagnostics.md`
- `docs/req-0019-recovery-and-lifecycle.md`
- `docs/req-0023-architecture-and-security-consistency.md`
- `docs/ts-0001-test-strategy.md`
- `wsp/security/security-requirements.md`
