# WPM Design for Security (DFS)

**Content type:** Controlled security design

**Status:** Accepted

**Owner:** WPM maintainers

## 1. Security scope

This DFS covers the WPM executable, package and repository parsers, archive
staging, package-index validation, signing and trust operations, package-script
invocation, installed archive and audit storage, upgrade handoff, build inputs,
and release production.

The intended environment is supported Windows 10 and Windows 11 on x86, x64,
and ARM64. Installation, removal, trust-store mutation, and system-wide upgrade
are privileged administrative activities.

### Protected assets

| Asset | Required protection | Adverse consequence |
| --- | --- | --- |
| WPM executable and release packages | Authenticity and integrity | Execution of substituted package-manager code |
| Package signing private keys | Confidentiality and authorized use | Forged packages accepted under a trusted identity |
| Trusted and revoked public-key records | Integrity and authorized mutation | Unauthorized trust or reactivation of a revoked signer |
| Package archives and indexes | Integrity, authenticity, and bounded processing | Tampered content, path escape, or resource exhaustion |
| Repository configuration and caches | Integrity and provenance | Unintended source selection or stale/misleading candidates |
| Installed-package archive records | Integrity and availability | Incorrect removal, upgrade, or installed-state decisions |
| Audit and failure records | Integrity and availability | Lost attribution, diagnosis, or recovery evidence |
| Credentials and proxy or repository secrets | Confidentiality | Unauthorized external access |

### Assumptions

- The operating system, administrator account, filesystem security, TLS stack,
  GitHub release service, and reviewed cryptographic dependencies behave as
  documented.
- Administrators establish signing-key trust through an independent and
  appropriate channel before relying on a package publisher.
- A trusted package script is intentionally granted the privileges of WPM.

### Explicit non-goals

- WPM does not sandbox, prove safe, or constrain the post-authorization behavior
  of package-provided scripts.
- WPM does not make a repository, HTTPS endpoint, archive filename, or successful
  parse a trust anchor.
- WPM does not provide universal rollback for arbitrary package scripts.
- WPM adoption of WSP does not claim security certification.

## 2. Security goals and derived requirements

| Goal | Derived requirements | Principal verification |
| --- | --- | --- |
| Keep archive writes inside controlled staging | REQ-0004, REQ-0012 | TC-0004, TC-0012, source inspection |
| Detect missing, added, modified, or truncated package content | REQ-0005, REQ-0012 | TC-0005, TC-0012 |
| Execute packages only after required authorization and validation | REQ-0004, REQ-0012 | TC-0004, TC-0012 |
| Separate repository transport from package trust | REQ-0011, REQ-0012 | TC-0011, TC-0012 |
| Prevent silent architecture changes, downgrade, or invalid upgrade state | REQ-0013 | TC-0013 |
| Preserve installation, failure, signer, and upgrade evidence | REQ-0012, REQ-0013 | TC-0012, TC-0013 |
| Protect release identity and signing inputs | REQ-0009, REQ-0012 | TC-0009, TC-0012, workflow inspection |

These project requirements derive the applicable `WSP-SEC-0001` through
`WSP-SEC-0014` obligations into the WPM baseline. Requirement changes follow
the controlled traceability and impact-analysis process.

## 3. Trust model

### Trusted components and data

- the exact WPM executable selected by the administrator;
- the supported Windows security and filesystem primitives used by WPM;
- pinned and recorded build dependencies;
- public keys explicitly present in the active trust store; and
- protected release infrastructure and signing identity within their defined
  operating procedures.

### Partially trusted components

- HTTPS and GitHub provide authenticated transport for their endpoint but do
  not authorize a package signer;
- repository indexes and caches may guide discovery only after applicable
  validation;
- installed archive records describe local state but require full validation
  before reused content executes; and
- package metadata is trusted only for the operation and validation level that
  consumed it.

### Untrusted inputs

- package archives, indexes, signatures, metadata, and scripts before validation;
- repository responses, redirects, cache contents, filenames, and URLs;
- command-line arguments, environment overrides, configuration, and paths;
- package names, versions, architectures, and script output; and
- prior or externally modified WPM data-directory contents.

### Trust boundaries and privileged operations

| Boundary | Entry points | Required decision before crossing |
| --- | --- | --- |
| Network to repository cache | HTTPS repository commands | TLS validation, response bounds, format validation, and cache isolation |
| Archive to staging filesystem | Build, install, verify, remove, and upgrade | Canonical path containment, type/size checks, and controlled extraction |
| Staged content to execution | Install, remove, and upgrade scripts | Index validation, signature/trust policy, metadata validation, and explicit operation |
| User context to machine trust | `wpm trust add/revoke` | Administrative authorization and exact key identity validation |
| CI source to published release | Release workflow | Pinned source/dependencies, matrix gates, protected signing input, and artifact verification |
| Current process to upgrade handoff | WPM self-upgrade | Candidate validation, cached handoff identity, audit logging, and recovery record |

Network location, repository ownership, successful TLS, file ownership, or
successful parsing alone never crosses a package-trust boundary.

## 4. Threat analysis and control traceability

| Threat | Scenario and consequence | Preventive/detective controls | Verification | Residual risk |
| --- | --- | --- | --- | --- |
| WPM-THR-001 Archive path escape | Traversal, absolute, UNC, reserved-device, alternate-separator, or canonicalization tricks write outside staging | Reject unsafe entry names; canonical containment checks; bounded staging root | TC-0004, TC-0012; archive-source inspection | OS/filesystem parsing differences may expose untested aliases |
| WPM-THR-002 Package tampering | An attacker changes, adds, removes, or truncates archived content | BLAKE2b file index, size checks, completeness validation, Ed25519 signature over the index | TC-0005 and TC-0012 | A trusted signer can intentionally publish harmful content |
| WPM-THR-003 Repository compromise | A repository serves substituted metadata or packages | HTTPS transport, package signature verification, explicit local signer trust, offline verification | TC-0011 and TC-0012 | Availability, hiding, replay, and stale-cache attacks remain possible within version policy |
| WPM-THR-004 Signing-key compromise | A stolen private key signs malicious packages | Private keys stay outside the repository; protected release secret; explicit revocation; durable key IDs | TC-0009, TC-0012; release inspection | Previously trusted packages and undiscovered compromise remain risks |
| WPM-THR-005 Trust-store mutation | An unprivileged actor adds or reactivates a key | Administrative authorization, separate active/revoked records, exact key-ID validation | TC-0012 | A compromised administrator or OS can alter local trust |
| WPM-THR-006 Malformed or excessive input | Crafted archives, metadata, URLs, indexes, or arguments cause memory, disk, or CPU exhaustion or parser confusion | Length/range/format checks, bounded metadata reads, controlled temporary roots, fail-closed parsing | TC-0002 through TC-0006 and TC-0010 through TC-0013 | Very large valid packages can still consume substantial resources |
| WPM-THR-007 Privileged script abuse | An authorized package script modifies arbitrary system state | Validation before execution, explicit operation/warning, signer attribution, audit records | TC-0004, TC-0012, TC-0013 | Script behavior is intentionally outside the WPM sandbox boundary |
| WPM-THR-008 Downgrade or candidate confusion | Architecture, prerelease, priority, or SemVer ambiguity selects an unintended package | Exact identity rules, SemVer precedence, architecture filtering, controlled prerelease policy, explicit downgrade rules | TC-0011 and TC-0013 | Repository freshness and operator override can affect selection |
| WPM-THR-009 Interrupted upgrade | Script or handoff failure leaves deployed software and WPM records inconsistent | Prevalidation, retained archives, failure audit, best-effort continuation rules, explicit recovery state | TC-0013 and previous-release upgrade verification | Arbitrary scripts prevent universal transactional rollback |
| WPM-THR-010 Secret disclosure | Keys or credentials appear in source, logs, arguments, artifacts, or diagnostics | No committed release private key; memory clearing for key material; protected CI secret; diagnostic review | TC-0009, TC-0012; secret and artifact inspection | Endpoint compromise can expose secrets during authorized use |
| WPM-THR-011 Build or dependency substitution | Modified compiler, runtime, submodule, or dependency changes release behavior | Pinned submodules, recorded dependency versions, controlled setup, architecture matrix, release verification | TC-0009; workflow and dependency inspection | External package distribution remains a supply-chain dependency |
| WPM-THR-012 Evidence deletion or forgery | Audit or test evidence is altered, omitted, or overwritten | Fail-closed audit writes where required, retained failure evidence, immutable CI/release association, artifact checksums/signatures | TC-0012, TC-0013; release-record inspection | A local administrator can alter local audit files |

## 5. Security control design

### 5.1 Archive and resource control

WPM extracts only into an operation-specific staging directory. It validates
archive entry structure and canonical containment before a path affects the
filesystem. Package metadata reads use explicit bounds, including the 1 MiB
metadata-only inspection limit. Validation failure aborts before package-script
execution and produces a nonzero result.

Temporary and cached content belongs beneath controlled WPM data roots. Cleanup
is attempted after success and failure; retained diagnostics are kept when they
are required for recovery or defect evidence.

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

Unsigned installation is denied by default. The `--allow-unsigned` exception
requires an explicit request and an authorized context, emits a warning, and is
recorded as unsigned. It is a deliberate administrative bypass, not trust.

### 5.4 Repository and cache security

HTTPS repositories use the Windows TLS validation stack. TLS authenticates the
configured endpoint and protects transport; package signatures and the local
trust store authorize package content. Repository priority, architecture,
version, cache, and offline rules are deterministic and covered by controlled
tests. Unavailable or malformed sources fail without authorizing unverified
content.

### 5.5 Least privilege and package scripts

Read-only inspection and verification do not intentionally modify package,
trust, or audit state. Machine trust mutation and privileged unsigned-package
exceptions require authorization. WPM performs validation before invoking
install or removal scripts and attributes resulting records to package and
signer identity.

WPM cannot reduce the privileges of an administrator-approved script without a
separate sandbox and compatibility model. This accepted boundary is visible in
warnings, documentation, and residual-risk decisions.

### 5.6 Logging and protected data

Installation and upgrade records include package identity, version transition
where applicable, verification outcome, signing-key identity or unsigned state,
timestamp, and failure stage or script exit status when relevant. Logs shall not
contain private-key material, credentials, or avoidable protected data.

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

### 5.8 Build and release integrity

Release workflows check out recursive pinned submodules, provision recorded
tool and dependency versions, test x86/x64/ARM64 configurations, protect signing
material from artifacts and logs, verify signed output, and gate publication on
required verification and previous-release upgrade results.

Known dependency vulnerabilities and material toolchain changes are assessed as
part of release review and WSP baseline upgrades.

## 6. Residual-risk decisions

| Risk | Decision | Basis and compensating control | Review trigger |
| --- | --- | --- | --- |
| Trusted package scripts have administrator-equivalent effect | Accepted | Explicit trust, validation, warning, signer attribution, and audit; sandboxing is a non-goal | New sandbox requirement or material abuse case |
| Local administrators can alter trust and audit files | Accepted | Administrative authorization is the local security boundary; release evidence is separately protected | Multi-user trust policy or remote administration feature |
| No universal rollback for arbitrary scripts | Accepted | Prevalidation, retained archives, failure audit, and package-specific recovery | Direct-install or transaction metadata design |
| HTTPS and repositories can withhold or replay available content | Accepted with controls | Signatures prevent unauthorized content; SemVer and explicit policy constrain selection | Repository snapshot or transparency-log design |
| CI cannot cover every Windows patch, policy, filesystem, or antivirus product | Accepted | Architecture matrix, supported-platform policy, fault injection, and field defect response | New support claim or compatibility defect pattern |

No residual-risk decision authorizes bypassing a required release gate without
the deviation record defined by `docs/ts-0001-test-strategy.md`.

## 7. Security verification

Security verification combines negative automated tests, fault injection,
source and workflow inspection, dependency review, traceability validation, and
architecture-specific release execution. Applicable security requirements and
accepted threats map to the evidence above.

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
- `docs/req-0004-package-installation.md`
- `docs/req-0005-package-index-signature-verification.md`
- `docs/req-0009-github-release-artifacts.md`
- `docs/req-0011-https-repositories.md`
- `docs/req-0012-package-signing-and-validation.md`
- `docs/req-0013-version-aware-upgrades.md`
- `docs/ts-0001-test-strategy.md`
- `wsp/security/security-requirements.md`
