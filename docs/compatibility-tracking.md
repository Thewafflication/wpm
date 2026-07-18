# Compatibility Tracking

This document tracks compatibility investigations that may influence future
releases. Entries here are not support commitments unless they are also listed
in the support policy and release gate for a published version.

## Legacy x86 Windows investigation

### Target under investigation

- Processor: x86, minimum Intel Pentium III-class capability.
- Operating system: Windows 2000 SP4.
- Delivery: a locally authored repository on CD-ROM, USB media, fixed disk, or
  SMB share; plain HTTP may be used where HTTPS support is impractical.

### Status

Tracked for investigation. This is not a WPM 2.0 support commitment and does
not change the supported-platform statement in the support policy.

### Questions to resolve

- Can the selected compiler, C runtime, and bundled dependencies produce an
  executable whose imported Windows APIs are available on Windows 2000?
- Can all x86 release paths avoid processor instructions newer than Pentium III?
- Which repository transports and cryptographic operations work reliably on
  Windows 2000, including certificate and TLS limitations?
- Can package signing, archive validation, installation, removal, and local
  repository use be exercised on a real or faithfully emulated Windows 2000
  system?
- What installer, path, filesystem, code-page, and removable-media constraints
  need explicit documentation?

### Evidence required before commitment

- A reproducible x86 build configuration.
- Import-table and CPU-instruction compatibility checks.
- An execution report from a clean Windows 2000 SP4 environment.
- An explicit security review of enabled transports and trust behavior.

## Recording updates

For each investigation, record the WPM version, build configuration, operating
system image or hardware, transport used, commands exercised, outcome, and any
known limitations. Promote an entry to a supported platform only through an
approved support-policy and release-roadmap change.
