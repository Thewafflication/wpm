# WPM Commit Checks

**Content type:** Contributor procedure and check inventory

WPM pins WSP 1.4.0. `.pre-commit-config.yaml` runs lint, build, then fresh
tests once for every commit, including documentation-only and empty commits.
Failures stop the commit. Hooks never install dependencies or fix sources.

## Setup and Manual Execution

Use x64 Windows with Python 3.9 or newer, PowerShell 7, Git, CMake, Ninja,
TinyCC, WCRT 1.3.1 or newer, and Clang/LLVM as described in the README.
Initialize recursive submodules. Use the same tested TinyCC/WCRT packages as
your development build; set `WPM_TCC_ROOT` and `WPM_WCRT_ROOT` to select them.
Set `WPM_NINJA` when Ninja is outside PATH. Full toolchain version locking is
still deferred in the adoption record.

From the repository root:

```powershell
git submodule update --init --recursive
python -m venv out/commit-hooks
out/commit-hooks/Scripts/python.exe -m pip install pre-commit==4.3.0
out/commit-hooks/Scripts/python.exe -m pip install `
    -r tools/requirements-style.txt
out/commit-hooks/Scripts/python.exe -m pre_commit install
out/commit-hooks/Scripts/python.exe -m pre_commit run --all-files
```

Run the canonical checks independently:

```powershell
pwsh -NoProfile -File tools/Invoke-CommitCheck.ps1 -Stage Lint
pwsh -NoProfile -File tools/Invoke-CommitCheck.ps1 -Stage Build
pwsh -NoProfile -File tools/Invoke-CommitCheck.ps1 -Stage Test
```

Stage the updated `wsp` gitlink together with its adoption record before
running lint: the existing adoption validator checks the Git index pin.

The build uses `x64-debug`, enables `BUILD_TESTING`, and disables build-time
tests and cached report generation. Its dedicated build directory avoids
reusing another preset's cached compiler. Outputs stay in
`out/build/commit-x64-debug`
and the existing project-owned `bin/x64/Debug` location. The test stage runs
all registered tests with unfiltered CTest and `--no-tests=error`; it does
not reuse report evidence. It includes TC runners, component tests, HTTPS
tests, and the traceability validator registered by `wpm/CMakeLists.txt`.
Proposed TC specifications without registered executable runners are outside
this current automated inventory. Network tests require their normal network
infrastructure. Missing tools, infrastructure, or unexpected failures block
commits. Negative tests pass only when their expected errors are asserted;
the wrapper never suppresses failed test exit codes. No blanket expected
failure exemption is configured.

## Check Inventory and Remaining Coverage

| Check | Scope and WSP allocation |
| --- | --- |
| Source style | Owned physical scan; C/CMake/Python formatter checks; PS syntax; strict JSON/YAML keys |
| C99 lint | Owned `wpm` C/H files; language edition, not complete C style |
| Traceability and validator tests | Controlled REQ/TC allocation; REQM and TEST |
| Adoption validator | Selected profile dispositions and staged WSP pin |
| Configure and build | Owned C/CMake plus pinned dependencies; compiler checks |
| Fresh CTest | All registered x64 Debug tests, including negative cases |
| Existing CI | Independent lint, builds, fresh CTest, verification and reports |

Vendored code under `third_party/`, pinned `wsp/`, and generated `out/` and `bin/` are
outside owned source-style migration scope. Owned scope includes `wpm/`,
`tests/`, `tools/`, `cmake/`, `.github/`, and root build/configuration files.
The physical scan covers 116 owned files, including the root batch installer
files and project-authored `third_party/CMakeLists.txt`. `tools/.cache/` is an
explicit tool-cache exclusion. Pinned clang-format, gersemi, Ruff, and PyYAML
versions are controlled in `tools/requirements-style.txt`; formatter settings
are controlled in `.clang-format`, `.gersemirc`, `.editorconfig`, and
`ruff.toml`. C documentation/Doxygen coverage, PowerShell analysis, and
remaining applicable language rules still need migration.
The existing adoption matrix records review/evidence controls and earlier
deferments; generic lint success does not prove those controls complete.
Unowned language profiles require explicit applicability review. Existing CI
retains its wider x86/x64/native ARM64 matrix and release controls. Its cached
verification reports are supplemented by unfiltered fresh CTest execution
after a successful verification build.

Completion of WSP-CHECK dispositions requires clean-setup verification,
controlled success/failure hook tests, documentation coverage, toolchain version
locking, and CI execution evidence. The adoption remains Proposed.
