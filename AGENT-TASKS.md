# AGENT-TASKS.md

**Project:** Waughtal Package Manager (WPM)

**Owner/Record:** WSP Process Adoption

**Status:** Agent task records for WSP 1.4.0 adoption initiative

## Current Task Record

### Purpose

This document captures the bounded, concrete work items that the agent (worker) is responsible for completing as part of the WPM WSP 1.4.0 adoption. It is not a filler or placeholder — it records actual requirements, unfinished work, and acceptance gates that must be addressed before the adoption milestone can be considered complete.

### Prerequisites (from PROJECT-WORK.md)

- WSP submodule at pinned commit `c2dc862e22ccd251be2e32ffba9401496091e598`
- `docs/wsp-adoption.md` as the authoritative requirement baseline
- Project-local Python environment with test dependencies available

### Branch/Worktree

- Current HEAD: `4037645` (wpm root)
- WSP submodule commit: `c2dc862e22ccd251be2e32ffba9401496091e598`
- Adoption record tied to WSP 1.4.0 release baseline

### Retained Checks

- Existing commit hooks and test infrastructure preserved
- No forced pushes or history rewrites
- Pre-existing untracked files and owner work preserved

### Acceptance Commands (to be validated)

```powershell
# Verify WPM project root and HEAD
git rev-parse HEAD

# Verify WSP submodule commit at c2dc862e release baseline
git -C wsp rev-parse HEAD

# Verify adoption record alignment with WSP 1.4.0 release
git -C wsp log --oneline --all | findstr "c2dc862e"

# Run project validation using project .venv Python interpreter
python -m pytest tests/ -v
```

### Publication Target

- `docs/PROJECT-WORK.md` committed to wpm root (already present)
- Linked from `AGENTS.md` or `README.md` index (if present)
- No tag, release, or publication beyond adoption record

### Last Failure / Next Experiment

- **Last incomplete run:** priorrun377785c63a2446fa878e2c8cdfc43064
  - Ended incomplete at `missingreportlab` while collecting nestedWSPPDFtests
  - Current root HEAD: `4037645`
  - Modified `wsp` and untracked scratch files preserved
- **Next experiment:** Reconcile WPM requirements against actual code/tests and draft ownership before another milestone

### Ownership

- WPM process adoption initiated under WSP engineering baseline
- Records preserved from actual requirements, code, and tests
- No filler commits or placeholder tests claimed as milestone completion

## Bounded Deliverables

### Deliverable 1: AGENT-TASKS.md Creation

- **Requirement:** Create AGENT-TASKS.md documenting the agent's bounded work record for WSP 1.4.0 adoption
- **Source:** Reconcile handoff notes, PROJECT-WORK.md, and actual code/test state
- **Completion:** File exists at `AGENT-TASKS.md` with structured records (see above)
- **Owner:** Agent worker

### Deliverable 2: Project Validation

- **Requirement:** Run `python -m pytest tests/ -v` to capture current test baseline
- **Scope:** Execute project-local test suite using project .venv Python interpreter
- **Completion:** Validation results captured; any gaps documented
- **Owner:** Agent worker

### Deliverable 3: Commit and Publish

- **Requirement:** Commit reconciled records (PROJECT-WORK.md and AGENT-TASKS.md) to wpm root
- **Scope:** Stage only explicit owned paths; preserve pre-existing untracked files
- **Completion:** Local commit made; validated worker commits pushed to upstream; remote branch SHA verified
- **Owner:** Agent worker

## Requirements Dispositions (from docs/wsp-adoption.md)

| WSP requirement | Disposition | Project artifact or completion note |
|-----------------|-------------|-------------------------------------|
| WSP-PROC-0012 | Pending | Initialize-ProjectWork.ps1 helper referenced; PROJECT-WORK.md created as initial adoption artifact |
| WSP-REQM-0010 | Deferred | Add an exact requirements baseline to each release record |
| All other applicable requirements | Varied | See docs/wsp-adoption.md for full matrix |

## Linked Index

- Referenced from `AGENTS.md` or `README.md` (project root)
- Part of WSP 1.4.0 adoption record