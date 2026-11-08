# Windows 2000/XP file-user discovery

`wpm file-processes` automatically uses its legacy implementation when the
system has no Restart Manager DLL. Use `--legacy` to exercise it on a newer
Windows host. No PowerShell, .NET, WMI, external handle utility, or driver is
required by the command. Use the native-architecture WPM executable, elevated
when inspecting other users' applications.

The backend obtains a bounded NT handle-table snapshot, preferring information
class 64 and falling back to class 16 on Windows 2000. It identifies the file
object type from its own metadata handles, duplicates candidate file handles
without closing the originals, and compares volume serial/file IDs. Its own
temporary metadata handles are excluded. Tool Help snapshots also find loaded
EXEs/DLLs that have no persistent file handle. Processes are deduplicated and
their creation times are retained for the shared termination guard.
The Windows 2000 class-16 table has 16-bit PID/handle fields; very large values
can be omitted. Failure to identify the scanner's own file type is an error.
Filesystems that return a zero file ID are refused rather than matched broadly.

The backend never queries object names and skips non-disk handles, including
pipes. SCM enumeration protects service PIDs; failure to inspect the SCM aborts
kill mode. Known NT system-image names, installer ancestors, the querying
process, and available BreakOnTermination flags are protected. The legacy scan
warns when it cannot inspect a process, including cross-architecture modules.
It does not detect every kernel or arbitrary memory-mapping file user and does
not guarantee that the following copy will succeed. Disk/network I/O can be
slow. The normal explicit-kill, dry-run, exit-wait, and copy-error rules apply.

## Automated checks on current Windows

Build `wpm` and `wpm-file-holder-test` with `BUILD_TESTING=ON`, then run:

```powershell
ctest --test-dir out/build/commit-x64-debug --output-on-failure -R '^file-processes(-legacy)?-integration$'
```

The integration runner uses a native same-architecture holder to exercise an
unshared data-file handle and a private loaded DLL. It verifies duplicate-path
listing, dry-run, ancestor protection, unrelated-process preservation, explicit
termination, and replacement after exit. The native holder also works without
Python on legacy Windows. The x86 PE checker verifies header/import constraints;
it does not establish that the complete executable runs on an old OS.

## Native Windows 2000 SP4 / XP smoke procedure

Copy the built x86 `wpm.exe` and `wpm-file-holder-test.exe` to an isolated local
directory on the test system. In a disposable administrator test session:

```bat
echo old version>payload.dat
start "file holder" /b wpm-file-holder-test.exe hold payload.dat
wpm file-processes payload.dat
wpm file-processes payload.dat --kill --dry-run
wpm file-processes payload.dat --kill
echo new version>payload.dat
start "DLL holder" /b wpm-file-holder-test.exe module private-version.dll
wpm file-processes private-version.dll
wpm file-processes private-version.dll --kill
del private-version.dll
```

Wait for each holder to print `ready` before querying. Confirm list and dry-run
preserve it, explicit kill releases it, and replacement/deletion succeeds. Also
exercise a second unrelated holder and a parent holding a target, verify the
parent is refused, and record any inspection warnings. Record the OS/service
pack, architecture, build/source identity, PE results, commands, output, exit
codes, and replacement results. Real XP/2000 execution remains unverified until
this procedure is run there; current-host forced-backend checks are distinct
evidence and do not change the project's platform support policy.

The ABI/API sources are the
[System Informer NT declarations](https://github.com/winsiderss/systeminformer/blob/master/phnt/include/ntexapi.h),
[Microsoft Tool Help documentation](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot),
[file identity API](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileinformationbyhandle),
and [service enumeration API](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-enumservicesstatusexa).
