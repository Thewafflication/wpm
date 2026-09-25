# Mozilla CA snapshot

`cacert.pem` was obtained from https://curl.se/ca/cacert.pem on 2026-09-24.
Its header identifies Mozilla certificate data dated 2026-08-13.
SHA-256 of the downloaded file:
`f66dff1bdf8f96060b8177976f8b7d9254bc89bc4db933d769f7384d28480bc9`.

Source and extraction procedure: https://curl.se/docs/caextract.html.
The original PEM file is retained here; CMake embeds its certificate blocks
in WPM. This file is public trust-anchor data, not a private key.

This Source Code Form is subject to the terms of the Mozilla Public License,
v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain
one at https://mozilla.org/MPL/2.0/.

To update: obtain the current official HTTPS snapshot, inspect the certificate
changes, replace the PEM, update this date/hash, rebuild all targets, and run
the local HTTPS tests plus a verified GitHub release-asset download. No CA data
is fetched during configuration/build or updated silently at runtime.
