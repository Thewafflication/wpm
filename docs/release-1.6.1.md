# WPM 1.6.1 - release bootstrap compatibility

This release includes the dependency repository hints and reviewed roadmap
reconciliation described in [the 1.6.0 scope](release-1.6.0.md).
The 1.6.0 tag remains retained as a failed release attempt; no release assets
were published from it.

Native verification failed before compilation while the shared setup action
bootstrapped the existing 1.5.1 WPM release. Windows PowerShell inherited the
runner's PowerShell 7 module search path; archive extraction reported no
three-argument ExtractToDirectory overload. The setup action now clears
PSModulePath for the Windows PowerShell bootstrap child and restores it in
finally for the remaining PowerShell 7 toolchain work. Package signature
validation, download sources and the bootstrap exit-code gate are unchanged.

The original failure remains in release run 45080477917. Required commit hooks
and native release gates must pass for the new exact 1.6.1 source/tag; no
verification assertions or timeouts are waived. The maintainer's instruction
to commit and release authorizes this bounded release-environment fix.
