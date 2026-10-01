# WSP 1.4.0 Adoption Impact

**Content type:** Controlled change-impact record

**Date:** 2026-10-01

Upgrade WSP from 1.3.0 to release 1.4.0 at
`f009399dd1406219571ac978bfee85e85bbdfeac`.

The release adds common commit gates, physical source/configuration style,
and language profiles. WPM now controls ordered pre-commit lint/build/test
hooks using existing validators and an unfiltered fresh x64 Debug CTest run.
The adoption validator includes the new requirement sources. The adoption
record retains Proposed status and records every new requirement as Deferred
pending complete coverage and evidence; existing dispositions remain intact.

TC-0023 and the architecture/security baseline now identify the same reviewed
1.4.0 pin. TC-0023 checks the index pin and rejects unstaged submodule changes,
allowing staged WSP upgrades to execute their own pre-commit tests.

`docs/commit-checks.md` records setup, canonical commands, scope, exclusions,
and migration gaps. This change does not claim strict style compliance or
completion of reproducible toolchain and CI fresh-execution obligations.
Existing CI and release architecture checks continue as independent controls.
The test-report workflow additionally runs fresh unfiltered CTest after each
successful architecture verification build.

Verification uses the adoption and traceability validators, hook configuration
validation, and canonical local gates. Actual execution results and any local
infrastructure failures are reported with the change, without reclassifying
failures as expected. No product requirements or package formats change.

## Local Verification Results

On 2026-10-01, canonical lint passed, adoption validation covered 142
requirements, and the clean x64 Debug build passed with TinyCC
`0.9.28-rc.1448+72495402` and WCRT `1.3.1`. The build hook also passed through
pre-commit 4.3.0. Configuration ordering, complete-scope flags, and workflow
YAML were validated.

The unfiltered 33-test CTest run passed 32 tests and exposed TC-0023's old
pin/clean-HEAD assumption. After updating that check, its targeted rerun
passed, including the wrong-pin negative fixture and validator child tests.
Controlled isolated CTest fixtures verified rejection of unexpected failures
and empty inventories and acceptance of an explicit `WILL_FAIL` expectation.
The initial stale-compiler build failure propagated correctly. No commit was
created during verification. Complete real-commit acceptance coverage remains
deferred as recorded in the adoption matrix.

## Source Style Migration for 1.2.7

The physical source/configuration scan now covers every owned language and
rejects overlength lines, invalid UTF-8, trailing whitespace, and missing final
newlines. C, Python, and CMake formatters use controlled settings and pinned
versions. JSON uses two-space indentation. PowerShell syntax and duplicate
JSON/YAML keys are checked. These checks run before commits and in both
branch and release CI. Vendored sources, generated outputs, and the local
`tools/.cache/` cache remain outside owned scope. Strict Doxygen documentation
and complete language analysis remain Deferred.

Formatting preserves literals, URLs, protocol bytes, and command arguments.
PowerShell uses structured concatenation for long strings; batch registry
commands retain the external registry override through a shorter local name.
Static tests canonicalize formatted command text and match CTest registration
across whitespace while retaining negative validation and exact identifiers.
Python AST equivalence was checked for literal formatting. The native build
and installer/release-configuration tests pass after migration.
