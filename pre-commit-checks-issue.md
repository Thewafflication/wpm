# Run lint, build, and the full test suite before commits

Repository: `Thewafflication/wsp`

## Requested behavior

Before Git creates a commit, run the project's lint checks, build the project, and execute the full test suite. The commit should proceed only when lint and build succeed and the test suite has no unexpected failures. Tests explicitly marked as expected failures count as acceptable results and must not block the commit.

## Implementation

Use the YAML-configured [pre-commit framework](https://pre-commit.com/) with a checked-in `.pre-commit-config.yaml` and hooks for the `pre-commit` stage. Reuse the project's canonical lint, build, and test commands. Configure project-wide build and test hooks to run once per commit, without limiting them to changed files or passing filenames to commands that do not accept them.

Document installation, `pre-commit install`, required development dependencies, and how to run the checks manually. Retain CI validation as a backstop because local Git hooks can be bypassed.

## Acceptance criteria

- [ ] A normal `git commit` runs lint, then a project build, then the full test suite before creating the commit.
- [ ] A lint failure, build failure, unexpected test failure, or test infrastructure error prevents the commit and produces actionable output.
- [ ] Explicitly expected test failures are accepted; ordinary failures are not suppressed or reclassified as expected.
- [ ] A successful lint/build/test run allows the commit to complete.
- [ ] Build and test checks run for every commit rather than being skipped by file matching.
- [ ] Contributor setup and manual execution are documented.
- [ ] Verify the hook with successful checks, a lint failure, a build failure, an unexpected test failure, and an expected test failure.
