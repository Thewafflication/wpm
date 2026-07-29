# WPM Project Process

**Content type:** Controlled project process

**Status:** Accepted

**Owner:** WPM maintainers

## 1. Purpose and scope

This process governs planning, requirements, architecture, implementation,
review, verification, release, support, security response, and improvement for
WPM. It applies to project-owned source, documentation, workflows, tests,
release artifacts, and adopted WSP obligations.

The process is proportional: a small documentation correction may use the
change description and review history as its record; a requirement, security,
compatibility, architecture, or release change uses the applicable controlled
records in `docs/process-record-templates.md`.

## 2. Lifecycle and required outputs

- **Adopt and baseline:** Select the WSP revision, profiles, dispositions, and
  tailoring. Control the result in `docs/wsp-adoption.md` and the `wsp/`
  gitlink.
- **Plan:** Define scope, risk, assumptions, dependencies, completion, review,
  and verification. Produce a work plan or equivalent change record.
- **Specify:** Create or update identified, verifiable requirements and
  traceability in `docs/req-*.md` and `docs/traceability-1.0.md`.
- **Decide and design:** Record durable decisions and security effects in
  `docs/adr-*.md` and `docs/dfs.md`.
- **Implement:** Change controlled source, scripts, build, workflow, or
  documentation through a reviewed repository change.
- **Verify:** Execute planned methods and preserve statuses, reports, and
  failures in TC specifications, runners, CI, and evidence.
- **Release:** Confirm readiness, approve the exact baseline, publish, and
  retain identity through release-readiness and release records.
- **Support:** Triage defects and vulnerabilities for supported releases using
  issue, defect, or security-finding records.
- **Improve:** Review release, incident, defect, and process evidence through
  retrospective and improvement records.

## 3. Roles and responsibilities

One person may hold multiple roles. The repository review and CI evidence make
the role performed and resulting decision visible.

| Role | Responsibility and approval |
| --- | --- |
| Maintainer | Configuration control, process ownership, WSP adoption, and final merge decisions |
| Requirement owner | Requirement quality, source, applicability, relationships, and change impact |
| Architecture owner | ADR consistency, interfaces, compatibility, and implementation allocation |
| Implementer | Scoped implementation, local checks, documentation, and disclosed assumptions |
| Verifier | Test design, objective evidence, failure preservation, and traceability review |
| Security owner | DFS review, sensitive finding control, severity, containment, and residual-risk approval |
| Release approver | Readiness review, exception approval, exact baseline, signing/trust evidence, and publication decision |
| Support owner | Defect/vulnerability triage, affected-version analysis, status, and response communication |
| Improvement owner | Retrospective facilitation, selected action ownership, and effectiveness check |

The author may verify ordinary changes. A security finding, trust-boundary
change, release exception, or accepted material residual risk requires explicit
maintainer approval in addition to author review. CI gates cannot be waived by
the author without a recorded emergency deviation and release approval.

## 4. Proportional planning

Before implementation, the change record identifies:

- objective, scope, exclusions, and completion criteria;
- novelty, risk, affected users/releases, and security or compatibility impact;
- dependencies, assumptions, uncertainty, and required external coordination;
- affected requirements, ADRs, DFS content, tests, documentation, and evidence;
- required reviewers, verification methods, configurations, and release effect.

Low-risk changes may keep this information in the commit or pull-request
description. Material changes use the Work Plan template. Assumptions are
revisited when evidence contradicts them or scope changes.

## 5. Requirements and architecture control

Requirements follow the structure and identifier rules enforced by
`tests/verify-traceability.ps1`. A controlled requirement change includes an
impact analysis before approval. Removed identifiers remain in Git history and
are never reused.

Durable or difficult-to-reverse decisions use the WSP-aligned ADR structure.
Accepted ADR substance changes through a superseding ADR, not silent rewriting.
Tests normally trace to requirements; architecture inspection is added when the
property itself requires verification.

## 6. Change-impact analysis

The author performs impact analysis for changes to accepted requirements, ADRs,
interfaces, package formats, trust boundaries, security controls, tests,
supported platforms, dependencies, release workflows, and published artifacts.

The analysis covers, as applicable:

- dependent or conflicting requirements and WSP dispositions;
- design, implementation, data formats, and migration;
- test specifications, matrix entries, retained evidence, and regression risk;
- compatibility, security, support, documentation, schedule, and release scope;
- deployed versions, rollback or recovery, users, and external coordination.

An unexplored applicable category blocks approval. A category may be marked Not
applicable only with a short rationale.

## 7. Review and approval

Review inputs are the proposed diff, plan/impact analysis, related requirements,
ADRs and DFS, test changes/results, generated artifacts, and known findings.
Review depth follows risk and novelty.

Completion requires:

1. required artifacts and traceability are present;
2. applicable automated gates pass;
3. findings are resolved, explicitly deferred, or accepted as risk;
4. assumptions, limitations, and release effects are visible; and
5. the approving maintainer records acceptance through the repository change.

A material finding records its artifact/location, priority or severity, owner,
resolution, verification, and approver. An author does not close their own
material finding without explicit approval.

## 8. Issue, defect, and security-finding control

Records contain observed and expected behavior, affected baseline and versions,
environment, severity/priority, status, owner, evidence, resolution, and
verification. Controlled states are New, Triaged, In progress, Resolved,
Accepted risk, Deferred, and Closed.

Severity considers impact and likelihood:

- **Critical:** active or readily exploitable compromise with severe impact;
- **High:** credible compromise of trust, code execution, protected data, or
  release integrity;
- **Moderate:** meaningful impact with material prerequisites or containment;
- **Low:** limited impact or defense-in-depth weakness.

Security details, exploit material, credentials, private keys, and protected
reporter/user data remain in an access-controlled advisory or equivalent private
record. The public record contains only safe coordination information until
disclosure. `SECURITY.md` and the DFS govern vulnerability response.

## 9. Verification and evidence

`docs/ts-0001-test-strategy.md` governs test levels, specifications, statuses,
matrix execution, reporting, failure preservation, and retention. Required
verification is planned with the requirement and completed against the exact or
configuration-equivalent baseline claimed by the change or release.

No implementation, passing unrelated test, or manually inferred number match
substitutes for controlled traceability and objective pass criteria.

## 10. Release readiness and approval

Before publication, the release approver completes the Release Readiness record
and confirms:

- release scope, version, source revision, WSP pin, dependencies, and artifacts;
- requirement baseline and disposition of each applicable obligation;
- required matrix/evidence status and unresolved defects, vulnerabilities,
  deviations, risks, or limitations;
- documentation, notices, install/recovery guidance, compatibility, and support;
- package integrity, signing, timestamp, malware scan, provenance, and exact
  artifact digests when selected by the adoption record.

Every exception identifies its owner, rationale, impact, compensating control,
approval, and completion or review condition. A required unknown or non-Pass
gate blocks a verified release claim.

The Release Record captures approval, date, exact baseline, published artifact
identity, retained verification summary, and support status. Publication occurs
only after approval.

## 11. Support and security response

`docs/support-policy.md` defines supported versions and maintenance expectations.
`SECURITY.md` defines private reporting and coordinated disclosure. Field
failures feed affected requirements, DFS threats, tests, release decisions, and
process changes.

## 12. Retrospective and improvement

A retrospective is required after each material release, security incident,
failed release, escaped Critical/High defect, or process-gate failure. It occurs
before the next material release unless urgent response work documents a later
date and owner.

The review considers planning accuracy, escaped defects, findings, verification,
warnings, build/release quality, security, support, and evidence retention.
Selected improvements record owner, intended result, due/review condition,
approval, and an effectiveness measure. Reusable improvements are proposed to
WSP where appropriate.

## 13. Records and retention

Plans, impact analyses, reviews, defect/security records, release records, and
retrospectives are retained in Git, GitHub review/issue/advisory history, release
attachments, or another linked controlled system. Sensitive records use the
least access practical. Release and test evidence follows the retention period
in the test strategy.

## 14. Tailoring and emergency change

WSP tailoring is controlled only by `docs/wsp-adoption.md`. An emergency change
may abbreviate ordinary planning or review but shall record the reason, scope,
risk, approver, mandatory gates, follow-up owner, and completion condition.
Package trust, exact artifact verification, and safe handling of secrets cannot
be waived by an undocumented emergency.

## 15. References

- `docs/process-record-templates.md`
- `docs/wsp-adoption.md`
- `docs/ts-0001-test-strategy.md`
- `docs/dfs.md`
- `docs/support-policy.md`
- `SECURITY.md`
- `wsp/processes/software-lifecycle.md`
- `wsp/processes/project-process.md`
