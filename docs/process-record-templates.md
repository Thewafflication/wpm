# WPM Process Record Templates

**Content type:** Controlled templates

**Status:** Accepted

**Owner:** WPM maintainers

Copy the applicable template into a change, issue, advisory, release record, or
version-controlled project document. Replace every prompt; write `Not
applicable` with a rationale rather than silently omitting a field.

## 1. Work Plan

```text
Title and record identifier:
Owner:
Status: Proposed | Approved | In progress | Complete | Cancelled
Objective and completion criteria:
Scope and exclusions:
Affected baseline and releases:
Novelty, risk, and priority:
Dependencies and external coordination:
Assumptions and uncertainties:
Affected requirements, ADRs, DFS, tests, docs, and artifacts:
Verification methods and matrix:
Required reviewers and approvals:
Release effect:
Target or review condition:
```

## 2. Change-Impact Analysis

```text
Change identifier and summary:
Owner and date:
Changed controlled artifact:
Requirements and WSP dispositions:
Architecture, interfaces, formats, and migration:
Implementation and dependencies:
Tests, matrix, evidence, and retained prior results:
Compatibility and supported platforms:
Security assets, threats, boundaries, controls, and residual risk:
Documentation, notices, support, and users:
Released versions, rollback, and recovery:
Schedule or coordination impact:
New assumptions or uncertainty:
Overall risk and recommendation:
Required approvals:
```

## 3. Review Record

```text
Artifact/change and revision:
Author:
Reviewer and role:
Review scope and inputs:
Risk level:
Applicable completion criteria:
Automated and manual verification reviewed:
Findings (identifier, location, priority/severity, owner, status):
Deferred findings and completion conditions:
Accepted risks and approving authority:
Decision: Approve | Approve with follow-up | Reject
Decision date and evidence link:
```

## 4. Defect Record

```text
Identifier and title:
Reporter and date:
Affected version, revision, architecture, and environment:
Expected and observed behavior:
Reproduction steps and evidence:
Severity or priority and rationale:
Status and owner:
Affected requirements, tests, ADRs, DFS threats, and releases:
Root cause or analysis:
Resolution or accepted-risk rationale:
Corrective revision:
Verification and regression evidence:
Approver and closure date:
```

## 5. Security Finding Record

Store this record privately while details are sensitive.

```text
Identifier and safe title:
Reporter/contact and received date:
Confidentiality and disclosure status:
Affected versions, assets, boundaries, and WPM-THR identifiers:
Credibility, exploitability, impact, and likelihood:
Severity and rationale:
Indicators, reproduction, or proof-of-concept location:
Containment and signing/release-channel decision:
Owner and status:
Affected requirements, controls, tests, and releases:
Resolution or residual-risk decision:
Corrective revision and security-release plan:
Verification and regression evidence:
Disclosure coordination and advisory:
Approver, closure date, and follow-up:
```

## 6. Release Readiness Record

```text
Release version and proposed date:
Release approver:
Source revision, branch/tag, and WSP pin:
Dependency/toolchain baseline:
Requirements baseline and adoption-record status:
Published artifact inventory and expected names:
Architecture/OS/configuration matrix status:
Test, inspection, analysis, and report summary:
Unresolved defects, vulnerabilities, deviations, and residual risks:
Compatibility, support, and end-of-support effects:
Documentation, notices, installer, recovery, and communication:
Package signatures and integrity:
Authenticode, timestamp, Defender, provenance, and digests:
Rollback and emergency-response readiness:
Exceptions (owner, rationale, impact, control, approval, condition):
Decision: Ready | Not ready
Decision evidence and date:
```

## 7. Release Record

```text
Release version and date:
Approver and approval reference:
Source revision, tag, and WSP pin:
Requirements and dependency baseline:
Published artifacts (name, size, digest, signature/provenance identity):
Verification matrix and retained evidence locations:
Approved exceptions and review conditions:
Known limitations and support status:
Release URL and communication:
Post-publication verification:
```

## 8. Retrospective and Improvement Record

```text
Trigger and baseline:
Facilitator, participants, and date:
Planning accuracy and material assumptions:
Escaped defects and field/support outcomes:
Review findings and recurring causes:
Test, evidence, warning, and build quality:
Security and release outcomes:
Practices to retain:
Problems and contributing causes:
Selected improvements (identifier, owner, intended result, approval):
Due date or review condition:
Effectiveness measure and observation date:
WSP proposal needed: Yes | No, with rationale
Closure decision:
```

## 9. Emergency Deviation Record

```text
Identifier, owner, and date:
Emergency condition and affected release/change:
Abbreviated or bypassed ordinary activity:
Activities and gates that remain mandatory:
Risk, impact, and compensating control:
Approving authority:
Expiration or completion condition:
Follow-up work, owner, and verification:
Closure evidence:
```
