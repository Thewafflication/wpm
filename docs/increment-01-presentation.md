# I-01: Current Command Presentation

**Content type:** Controlled increment record

**Owner:** WPM maintainers

**Status:** Implemented; completion requires commit gates and native release matrix

## Scope and impact

Extends current public commands at the I-00 baseline. Existing task help, color
policy, package-script framing, operational logging, and result tests remain
the foundation. Package, signature, index, and installed-record formats are
unchanged. New recovery/authoring/query commands retain their later increments.

WPM diagnostics now carry explicit event class, stable code, operation, and
phase at their call sites. The human renderer and operational logger consume
the same filtered/redacted text. Event records retain severity and timestamp;
multiline messages produce separate attributed records. Public machine-output
schemas remain I-08 work. Package-script content remains opaque within existing
package/phase delimiters and cannot determine WPM's exit status.

Local repository copies now report bytes and completion using the shared
progress renderer. Archive creation/index preparation report lifecycle phases.
Verbose dispatch reports the selected command and diagnostic data locations.
Invalid architecture/SemVer selectors fail before initialization and logging.
Build has an explicit package/architecture final result. Interactive self-upgrade
keeps the returned prompt intact and records completion in its audit log.

## Verification allocation

- REQ-0014.001: existing TC-0027/TC-0030 current-command help partitions.
- REQ-0014.002/.003: TC-0026/TC-0034/TC-0036/TC-0037 and
  `presentation-integration` typed records and native-console auto styling.
- REQ-0014.004/.005: TC-0024/TC-0025, HTTPS integration, TC-0035, and
  local-copy/phase progress exercised by native self-upgrade fixtures.
- REQ-0014.006/.007: TC-0028 through TC-0032 and presentation integration
  credential/protected-field/control-sequence and pre-initialization selector
  partitions. Dry-run log suppression is verified in I-03.
- REQ-0014.008: TC-0033 script framing/content and exit-code-only result.

The new Python test starts a genuine Windows console hidden from the desktop,
uses native screen character/attribute reads, and proves that an isolated WPM
self-upgrade completes while the simulated shell prompt remains unchanged.
Only disposable data/package/script fixtures are used; no machine deployment
or environment-registry change occurs. CTest registers the test on each native
release verification architecture alongside the existing controlled TC runners.

## Completion and remaining allocation

Completion evidence is the required lint/build/full-test commit gates and the
tag-triggered x86/x64/ARM64 release matrix. Keep the REQ-0014 parent Proposed
until the composite controlled profiles, future applicable commands, and I-03
logging suppression are reconciled. This increment closes current-command
presentation work, not all future 2.0 release obligations. Quality-harness
protected-data review and public machine event serialization retain I-13/I-08.
