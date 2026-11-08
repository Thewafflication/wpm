# Confirmed recursive installer copy

The requested follow-up adds `wpm copy <source> <destination>` with equivalent
`--recursive` and `-r` options for directory payloads. It merges the contents
into the exact destination, retains destination-only files, and creates empty
subdirectories. Single-file sources also support exact destination filenames
and existing destination directories.

The default confirmation authorizes both forceful destination-file user
termination and copying/overwriting. Empty/negative input and EOF cancel with
a nonzero status; `--yes`/`-y` explicitly authorize unattended execution.
`--dry-run` previews without mutation. `--kill` is accepted but does not bypass
confirmation. Discovery and termination reuse the modern/legacy implementation
and its protected-process, creation-time and exit-wait checks. Formal platform
support is unchanged; native Windows 2000/XP execution remains unverified.

REQ-0013.041 through REQ-0013.043 and the TC-0013 companion default/legacy tests
cover the copy semantics, confirmation and path/error handling. Implementation
is allocated to `wpm/copy.c`, dispatched before package/data initialization.
Traversal is bounded to MAX_PATH, 128 levels and 65,536 entries; reparse paths,
overlaps (including expanded existing 8.3 aliases), same-file identities,
ambiguous/device/stream names and file/directory conflicts fail in planning.
Planned names/types are rechecked after confirmation. These are snapshot
checks; concurrent changes and inaccessible legacy processes can still cause
real copy errors. Native copy failures propagate nonzero and partial copies
are retained. No application restart or atomic rollback is promised.

Acceptance uses project style/C99/traceability checks, current-host x64 and
x86 builds, default/forced-legacy integration and the full x64 CTest suite.
The initial implementation was left for review. The user subsequently
authorized push and release; release 1.3.6 is subject to the normal commit
hooks, signed packaging, native architecture/upgrade tests and PDF gates.

## Verification results

- Project style, C99, traceability/validator and WSP adoption checks passed.
- The x64 Debug build passed; the full CTest suite passed 60/60 in 366.55 seconds.
- Both default and forced-legacy copy integrations passed on current-host x64
  and x86. Existing file-process integrations also passed in the full suite.
- The x86 Debug build passed the Windows XP PE check (OS/subsystem 4.0;
  128 kernel32 imports). This does not establish native XP/2000 execution.
- Regression cases include confirmation cancellation/EOF, dry-run, source
  changes while prompting, nested/empty folders, exact single-file targets,
  retained destination-only files, invalid/reserved/ambiguous paths, junctions,
  overlaps, hard links, protected ancestors and actual read-only copy failures.
