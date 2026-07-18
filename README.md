# WPM

WPM is the Waughtal Package Manager, a command-line tool for building,
installing, removing, and upgrading packages.

## WPM 1.0.0

WPM 1.0.0 is the first stable release of the Waughtal Package Manager for
Windows x86, x64, and ARM64. It provides signed package build, installation,
removal, HTTPS repository use, version-aware upgrades, and safe WPM
self-upgrade.

Use the latest-release installer below to install or upgrade WPM. See the
[support policy](docs/support-policy.md) for the scope of best-effort support,
and the [2.0 UX roadmap](docs/roadmap-2.0.md) for planned improvements.

## Install

From Command Prompt or PowerShell, download and run the latest bootstrap
installer:

```powershell
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$p=Join-Path $env:TEMP 'wpm-install.cmd'; try { Invoke-WebRequest -UseBasicParsing 'https://github.com/Thewafflication/wpm/releases/latest/download/install.cmd' -OutFile $p; & $p; exit $LASTEXITCODE } finally { Remove-Item -LiteralPath $p -Force -ErrorAction SilentlyContinue }"
```

The bootstrapper selects the native x86, x64, or ARM64 package from the latest
release index, downloads its release public key, validates the signed package
with the downloaded WPM executable in an isolated temporary trust store, and
then runs the packaged `setup.cmd`. Its initial trust anchor is GitHub HTTPS;
the public-key identifier is documented below for independent verification.

See the [usage guide](docs/usage.md) for commands, examples, and the WPM
package layout.

See the [1.0 release roadmap](docs/roadmap-1.0.md) for the 1.0 scope and
release criteria.

See the [2.0 UX roadmap](docs/roadmap-2.0.md) for the next major release.

See [compatibility tracking](docs/compatibility-tracking.md) for platforms
under investigation that are not yet supported releases.

See the [quality testing program](docs/quality-testing.md) for long-running and
fault-injection testing that complements regular CI.

See the [support policy](docs/support-policy.md) for supported releases and
issue-reporting guidance.

WPM uses third-party open-source software. See
[Third-Party Notices](THIRD_PARTY_NOTICES.md) for attribution and licenses.

## Official release signing key

Official WPM packages are signed with the durable public key in
[`release_keys/wpm-release.public`](release_keys/wpm-release.public). Verify
the key identifier before trusting it:

```text
6c21ebda96b16bfa64700d28fcdeb20dc4b19de448a723a970ac09b720f04b1d
```

After obtaining the key from a trusted project source, add it once with:

```text
wpm trust add wpm-release.public
```

WPM does not automatically trust keys distributed by package repositories.
