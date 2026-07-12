# REQ-0008: Package Removal

## Description

The `wpm remove` command shall remove a retained package archive identified by
its archive name, with or without the `.zip` extension. The archive name
identifies one exact stored package version.

When invoked with a stored package archive, the application shall:

- locate `%ProgramData%\WPM\packages\<archive-name>.zip`,
- extract the archive to `%ProgramData%\WPM\temp\<archive-name>`,
- verify the extracted package index before executing package removal logic,
- execute `.wpm\remove.cmd`, when present, with the staging directory as its
  working directory,
- retain the stored archive if extraction, verification, or `remove.cmd`
  fails,
- delete the stored archive after successful removal, and
- remove the staging directory after every removal attempt.

## Rationale

The retained archive provides the package content and removal script needed to
perform a version-specific removal after the original source archive is gone.
Keeping the archive when removal fails permits a later retry.

## Verification

Verified by:

- TC-0008 - Package removal
