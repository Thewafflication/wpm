# Installer file-user utility change record

Date: 2026-11-06

The requested increment lets package install scripts find and terminate running
applications using destination files before upgrading them. Completion requires
an opt-in native command, documented script usage and failure semantics, a
Windows build, and an integration check using a disposable file holder.

The interface adds `wpm file-processes <file...> [--kill] [--dry-run]`.
Existing installation, upgrade, package formats, trust validation, and defaults
are unchanged. Scripts supply their deployment destinations because WPM cannot
derive arbitrary script-controlled destinations from archive paths. No automatic
shutdown, application restart, service shutdown, directory recursion, or reboot
replacement is introduced.

REQ-0013.038 through REQ-0013.040 extend the upgrade requirement; TC-0013 has a
companion CTest integration runner. The implementation is allocated to
`wpm/file_processes.c`, dispatched before package data initialization. Restart
Manager is dynamically loaded from the system directory. The 2026-11-07 follow-up
adds Windows 2000/XP fallback discovery through NT handle tables and Tool Help
module snapshots, plus `--legacy` to exercise that backend on newer Windows.
Service PIDs are protected through dynamic SCM enumeration; known system images
and available critical flags supplement the shared identity/ancestor guards.
The ABI declarations avoid a newer SDK dependency. Inspection gaps are warned
and actual copy errors remain authoritative. See `file-processes-legacy.md` for
implementation limits and the native legacy verification procedure.
The source contracts are Microsoft's
[RmRegisterResources](https://learn.microsoft.com/en-us/windows/win32/api/restartmanager/nf-restartmanager-rmregisterresources),
[RmGetList](https://learn.microsoft.com/en-us/windows/win32/api/restartmanager/nf-restartmanager-rmgetlist),
[RM_PROCESS_INFO](https://learn.microsoft.com/en-us/windows/win32/api/restartmanager/ns-restartmanager-rm_process_info),
and [RM_UNIQUE_PROCESS](https://learn.microsoft.com/en-us/windows/win32/api/restartmanager/ns-restartmanager-rm_unique_process)
documentation.

Termination can lose unsaved work and requires explicit `--kill`. Creation-time
identity checks prevent PID reuse from targeting an unrelated process. Process
handles are held through termination and exit waits. The command refuses
services, Explorer, critical processes, itself, and installer ancestors.
Existing capability checks prohibit termination under read-only authority.
Protected targets and access failures are errors, never silently successful
upgrades. A batch can partially stop applications before another target fails;
scripts must abort on a nonzero exit and check subsequent copy errors. Discovery
is a snapshot and cannot prevent another process from opening a file later.

Verification covers exact-path discovery, duplicate resources, missing files,
invalid paths and options, list-only and dry-run preservation, explicit kill,
replacement after exit, and preservation of an unrelated same-executable
process. Inspection covers identity checks, protected exclusions, cleanup, and
legacy dynamic loading. Full x86/ARM64 release validation and legacy Windows
execution remain release-matrix responsibilities; local validation is not release
approval. No release publication is part of this change.

Local validation: canonical lint and x64 Debug build passed. The six selected
regression tests (file processes, dry-run, TC-0001, TC-0013, TC-0027, TC-0028)
passed, and the file-user test also passed with the ancestor-protection check.
The x86 Release build and the file-user integration test against that executable
also passed on the local Windows host.

The canonical full x64 test run completed with 55 of 57 tests passing.
`recovery-record-integration` exceeded its 120-second CTest limit on both the
full run and a focused retry, but passed when invoked directly. TC-0023 rejected
the pre-existing untracked Python cache directories in the `wsp` worktree.
Those caches and all other pre-existing work were preserved. These are failed
commit gates, not accepted exceptions: the change remains staged on
`codex/file-processes` without a commit, push, tag, or release.

The 2026-11-07 extension adds same-architecture native file/DLL holder fixtures
and a separate forced-legacy CTest entry. Current-host checks cover ordinary
file handles and loaded DLLs under both backends. This is not evidence of actual
Windows 2000/XP execution; those native smoke checks remain pending.

Follow-up validation passed on the current Windows host: x64 Debug and x86
Release builds; default and forced-legacy file/DLL holder checks; x86 forced
legacy checks; dry-run and TC-0013 regression checks; source style, C99, and
traceability validation; and x86 executable/holder PE checks. The prior complete
suite's timeout and pre-existing WSP worktree gate remain unresolved; a full
suite rerun and commit/publication are not claimed for this follow-up.
