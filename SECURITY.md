# WPM Security Policy

## Supported versions

Only the latest published WPM release receives best-effort security support.
Reporters should still identify every version known or suspected to be affected.

## Private reporting

Use GitHub's private vulnerability-reporting or security-advisory channel for
the WPM repository. Include:

- affected versions and architectures;
- the security boundary, asset, or trust decision involved;
- reproducible steps or a minimal proof of concept;
- expected and observed behavior;
- impact and known exploitation conditions; and
- suggested mitigations, if available.

Do not include private signing keys, credentials, personal data, or unrelated
machine contents. If GitHub private reporting is unavailable, open a minimal
public issue requesting private coordination. Do not publish exploit details in
that issue.

## Response and disclosure

Maintainers handle reports on a best-effort basis without a guaranteed response
or remediation time. They will assess affected versions, impact, exploitability,
urgency, release-channel or signing-key compromise, and required containment.

Please allow coordinated correction and verification before public disclosure.
An advisory will identify affected versions, mitigations, corrected releases,
verification information, and any required trust or key migration when it is
safe to publish.

## Security boundaries

Package scripts authorized by an administrator execute with WPM's privileges;
WPM does not sandbox them. Reports remain relevant when WPM executes a script
without the required validation or authorization, crosses its staging boundary,
accepts unauthorized package content, exposes protected material, or violates a
documented security control.

See `docs/dfs.md` for the controlled security design and threat model.
