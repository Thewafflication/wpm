# WPM 1.0 Requirements Traceability

This matrix is the release-baseline map from each normative requirement to its
test specification and automated implementation. CI also runs
`tests/verify-traceability.ps1`, which rejects missing, duplicate, unregistered,
or back-reference-free artifacts.

| Requirement | Test case | Primary command or release behavior |
| --- | --- | --- |
| REQ-0001 | TC-0001 | usage, version, and command-line invocation |
| REQ-0002 | TC-0002 | `wpm init` |
| REQ-0003 | TC-0003 | `wpm build` |
| REQ-0004 | TC-0004 | local `wpm install` |
| REQ-0005 | TC-0005 | package index and hash verification |
| REQ-0006 | TC-0006 | metadata and archive naming |
| REQ-0007 | TC-0007 | WPM self-installation and setup |
| REQ-0008 | TC-0008 | `wpm remove` |
| REQ-0009 | TC-0009 | tag-triggered release artifacts |
| REQ-0010 | TC-0010 | runtime mode and `--diagnose` |
| REQ-0011 | TC-0011 | repository and named installation commands |
| REQ-0012 | TC-0012 | signing, trust, validation, and audit commands |
| REQ-0013 | TC-0013 | version-aware installation, configuration, and upgrade |

The full TC-0001 through TC-0013 suite is required on x86, x64, and ARM64
Windows before release publication. Generated reports and execution evidence
from each architecture are retained as workflow artifacts.
