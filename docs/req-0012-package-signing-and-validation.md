# REQ-0012: Package Signing and Validation

## Description

WPM shall authenticate package contents with detached Ed25519 signatures before
installation. The existing package index remains the package-content manifest;
this requirement adds author identity and trust validation to it.

### Signed package format

A signed package shall contain `.wpm/index.csv` and a UTF-8
`.wpm/signature.json` file. `signature.json` shall use this version-1 schema:

```json
{
  "version": 1,
  "algorithm": "ed25519",
  "key_id": "<lowercase 64-character hexadecimal key identifier>",
  "public_key": "<base64-encoded 32-byte Ed25519 public key>",
  "signature": "<base64-encoded 64-byte Ed25519 signature>"
}
```

The signed payload shall be the exact bytes of `.wpm/index.csv` as stored in
the ZIP archive. The index shall list every archive entry except
`.wpm/index.csv` and `.wpm/signature.json`; WPM shall reject a signed package
when an entry is missing from the index or an unindexed entry is present. This
makes the signature cover the package metadata, scripts, and every deployable
file without a recursive signature structure.

The key identifier shall be the lowercase hexadecimal BLAKE2b-256 digest of
the 32-byte Ed25519 public key. WPM shall reject malformed signature metadata,
unsupported schema versions or algorithms, invalid Base64, duplicate signature
files, and a key identifier that does not match the public key used for
verification.

### Signing

`wpm keygen <private-key-file> <public-key-file> [--default]` shall generate a
new Ed25519 key pair. It shall require both parent directories to exist,
refuse to overwrite either output file, and restrict the private-key file to
the invoking user. It shall report the generated key
identifier and public-key file path, but shall never display the private key.
When `--default` is specified, WPM shall designate the generated private key
as the default signing key.

`wpm key default <private-key-file>` shall validate an existing private-key
file and designate it as the default signing key. `wpm key default --clear`
shall remove that designation. WPM shall store only the absolute private-key
path and the derived key identifier in `config\signing-key.txt` beneath the
WPM data root; it shall not copy or retain the private key. Default signing-key
configuration is independent of the machine trust store.

`wpm build <source_dir> [output_dir] --sign <private-key-file>` shall create a
signed package. It shall first generate the package index, sign its exact
bytes with the supplied Ed25519 private key, and add `signature.json` to the
archive. The build shall fail without producing a successful package if the
key file is malformed, unsupported, or cannot sign.

When `--sign` is omitted and a default signing key is configured, `wpm build`
shall use that key. `--sign` shall take precedence over the configured default.
When no default is configured, builds without `--sign` remain supported for
development and produce unsigned packages. If a configured default is missing,
malformed, or no longer matches its recorded key identifier, the build shall
fail rather than silently producing an unsigned package.

A private-key file shall be UTF-8 text in this format:

```text
wpm-private-key-v1
algorithm=ed25519
secret-key=<base64-encoded 64-byte Ed25519 secret key>
```

WPM shall not write, retain, display, or package private-key material except
when explicitly creating a new private-key file through `wpm keygen`.

### Trust store and management

Trusted public keys shall be stored under `trust` beneath the WPM data root
(`%ProgramData%\WPM` by default or `WPM_DATA_DIR` when overridden). WPM shall
provide these commands:

- `wpm trust add <public-key-file>` adds an active key.
- `wpm trust list` displays each key identifier and status.
- `wpm trust revoke <key-id>` permanently marks an existing key revoked.

Only an elevated administrator may add or revoke keys in the default data
root. When `WPM_DATA_DIR` explicitly selects an alternate data root, WPM may
modify that isolated trust store without elevation to support test automation
and non-machine deployments. A public-key file shall be UTF-8 text in this
format:

```text
wpm-public-key-v1
algorithm=ed25519
public-key=<base64-encoded 32-byte Ed25519 public key>
```

WPM shall derive the key identifier itself and shall reject duplicate active
keys, malformed key files, and attempts to revoke an unknown key. Revocation
shall be durable and a revoked key shall not be reactivated by `trust add`.
Private keys shall never enter the trust store. Adding a repository shall not
add or trust a signing key.

### Installation validation

Before extraction, script execution, or package-store retention, `wpm install`
shall validate each local or repository-downloaded package as follows:

1. Require exactly one well-formed signature file, unless the unsigned
   exception below applies.
2. Locate the declared public key in the local trust store and reject an
   revoked key. When the key is unknown, verify the declared public key and
   signature, then interactively prompt the user to trust it; a response other
   than `y` or `yes` rejects the package.
3. Verify the detached Ed25519 signature over the archive's exact index bytes.
4. Extract to the normal staging directory and verify the complete package
   index before running `.wpm\install.cmd`.

If any validation step fails, WPM shall return a nonzero exit status, execute
no package script, retain no package archive, and remove its staging directory.
Signature verification shall work without repository access once the package
and trusted public key are available.

### Read-only package verification

`wpm verify <package...>` shall apply the same signature, trust-store, complete
index, and package-metadata validation used before installation. It shall
reject unsigned packages and shall not provide an unsigned override. It shall
execute no package script, retain no package archive, write no installation or
upgrade audit record, and remove its staging directory after success or
failure. Each valid package shall produce a confirmation containing its
declared name, version, architecture, and signing-key identifier.

Unsigned packages shall be rejected by default. An elevated administrator may
install a package with no signature file only by specifying
`wpm install <package...> --allow-unsigned`. When `WPM_DATA_DIR` explicitly
selects an alternate data root, WPM may permit that exception without elevation
for test automation and non-machine deployments. WPM shall emit a prominent
warning for every package accepted this way. `--allow-unsigned` shall not
bypass a malformed, invalid, unknown-key, or revoked-key signature.

For each successful installation, WPM shall record the package name, version,
archive name, installation timestamp, signing-key identifier (or `unsigned`),
and verification result beneath the WPM data root.

## Rationale

A cryptographically signed package index authenticates both installation
scripts and package payloads. Trusting explicitly installed public keys,
rather than repositories, preserves verification when packages are downloaded
from a compromised source or handled offline. The narrowly scoped unsigned
exception supports development without allowing a signature failure to be
ignored. Built-in key generation gives package maintainers a complete signing
workflow without making WPM a private-key store, and default-key selection
avoids repeatedly passing a key path during routine builds.

## Verification

Verified by:

- TC-0012 - Package signing, trust, and installation validation
- the release workflow, which signs the WPM self-package with its release key
