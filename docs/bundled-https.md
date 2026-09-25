# Bundled HTTPS transport

WPM now uses an app-local Mbed TLS 3.6.7 TLS 1.2 client for HTTPS repositories
and package downloads. Mbed TLS and a pinned Mozilla CA snapshot are linked into
the executable. It does not install OneCoreAPI, replace Schannel, modify system
DLLs, or change the machine's certificate store. Package signatures remain
required under the existing trust policy.

The Windows adapter uses Winsock 2 IPv4 sockets, `gethostbyname`, and CryptoAPI
`CryptAcquireContextA`/`CryptGenRandom`. Time conversion uses
`FileTimeToSystemTime`. These APIs were selected for Windows 2000 compatibility;
compilation and import checks do not substitute for testing on actual Windows
2000/XP machines. The existing support policy remains unchanged until that
validation is complete. ReactOS also has an Mbed TLS Schannel implementation,
but WPM uses the upstream library directly and copies no compatibility-layer code.

## Verification and download behavior

- Server hostname, certificate chain and validity dates are verified on every
  connection, including each redirect. SNI is sent. There is no insecure mode
  and no automatic fallback to URLMon or HTTP after TLS failure.
- Only TLS 1.2 is enabled in this first implementation. Cipher suites use ECDHE
  with RSA/ECDSA authentication and AES-GCM or ChaCha20-Poly1305.
- HTTP/1.0 and HTTP/1.1 response framing supports content lengths, chunked bodies,
  and TLS-authenticated connection-close bodies. Truncated and ambiguous framing
  fails. Response headers, line lengths, trailer size, and redirect count are
  bounded. Up to ten HTTPS redirects may cross hosts; each new host is verified.
- Socket waits time out after 30 seconds; the handshake has a 60-second bound.
  DNS resolution uses Windows' resolver timeout. Downloads preserve 64-bit byte
  progress and replace the destination only after a complete successful response.
- An exclusively created, per-process temporary `.download` file is removed on
  failure. Existing destination
  contents survive certificate, framing, and HTTP-status failures.

## Configuration

The default is `WPM_HTTPS_BACKEND=bundled`. The CA bundle comes from
`third_party/certificates/cacert.pem`; its source, snapshot date, hash, and update
procedure are recorded beside it. Updating WPM refreshes the embedded snapshot
when maintainers update that source file.

For a private CA, set `WPM_TLS_CA_FILE` to a PEM file containing the intended trust
anchors. This **replaces** embedded trust. A missing, malformed, oversized, or
partially unparseable file fails closed. Keep that file under the same controls
as WPM's executable; it defines which HTTPS endpoints are trusted.

`WPM_HTTPS_BACKEND=urlmon` explicitly selects the original Windows transport for
environments that need its proxy integration. It uses Windows TLS/certificate
validation and therefore does not solve old Windows TLS limitations. A custom
CA-file setting cannot be combined with URLMon. `--version --verbose` reports
the backend and trust source.

This initial bundled backend supports direct IPv4 connections and ASCII DNS
names. It does not yet support IPv6 literals, proxy/PAC authentication, client
certificates, HTTP/2, TLS 1.3, or online OCSP/CRL fetching. It deliberately does
not import modern Winsock name-resolution APIs or BCrypt.

The bootstrap scripts still require their own HTTPS-capable downloader. For an
old Windows machine without one, transfer the release package from another
machine and install locally first; WPM's repository downloads then use bundled
TLS. Windows 2000 does not gain a PowerShell bootstrap from this change.

## Validation

`https-parser` checks URL/scheme/length parsing. `https-integration` creates an
ephemeral local CA and HTTPS server without modifying system trust. It checks
successful fixed/chunked/close-delimited bodies, redirects, interim responses,
wrong-host/expired/untrusted certificates, downgrade redirects, loops, malformed
headers, overflow lengths, and truncated bodies. Every negative case verifies
that the destination is preserved and no partial output remains. Tests require
PowerShell 7 and Python 3 on the build host, not on target legacy machines.

The live smoke test downloads a GitHub release asset with embedded trust and
cross-host redirects. Recheck this after Mbed TLS or CA updates. Mbed TLS security
updates must be maintained on the 3.6 LTS line and migrated before its support ends.
