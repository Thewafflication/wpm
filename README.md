# WPM

WPM is the Waughtal Package Manager, a command-line tool for building,
installing, removing, and upgrading packages.

See the [usage guide](docs/usage.md) for commands, examples, and the WPM
package layout.

See the [1.0 release roadmap](docs/roadmap-1.0.md) for planned capabilities
and release criteria.

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
