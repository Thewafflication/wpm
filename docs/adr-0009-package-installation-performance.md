# ADR-0009: Package installation performance

## Status

Accepted.

## Context

WPM installs a package by extracting it to a staging directory, validating its
signature and per-file BLAKE2b index, running the package-provided install
script, retaining the original archive, and deleting the staging directory.

Large application packages make the cost of this safety model visible. For
example, MyPal 78.0.3 is a 64 MiB archive containing 86 files and approximately
134 MiB of uncompressed data. Installation writes the staging payload, reads it
again for hashing, and its current install script copies the payload to its
final location. Antivirus software can inspect both sets of newly created
executables and libraries.

Two avoidable costs also existed in WPM itself:

- parent directories were walked and checked again for every extracted file,
  even for consecutive files in the same directory; and
- completeness validation reopened and scanned `.wpm/index.csv` once for every
  extracted file, producing quadratic file-I/O growth with package file count.

Renaming a directory within one Windows volume is normally a metadata operation
and can eliminate a full payload copy. WPM does not, however, own or know a
package's final location. It is chosen by the package's arbitrary `install.cmd`,
and WPM has no generic transaction or rollback contract for partially installed
files.

## Decision

WPM SHALL:

1. remember the most recently created extraction parent directory and avoid
   repeating its directory walk for consecutive files;
2. load indexed paths into memory once, sort them case-insensitively, and use
   binary lookup while checking completeness; and
3. retain staging extraction and verification before script execution.

Packages that deploy a directory tree SHOULD place deployable files beneath a
top-level `payload` directory while keeping `.wpm` beside it. After verification,
`install.cmd` MAY rename `payload` into its final location when staging and the
destination are on the same volume. It SHOULD first move an existing destination
aside, restore it if installation fails, and remove it only after the new payload
is in place. Copying remains the required fallback across volumes.

The package MUST NOT rename the staging root because it contains the executing
script and is owned by WPM. A future WPM-managed direct-install feature requires
explicit destination and rollback metadata; it must not infer those properties
from package-specific script contents.

## Consequences

Completeness checking changes from repeatedly reading the index for every file
to one index read plus in-memory `O(log n)` lookups. Common directory layouts
perform fewer Windows filesystem metadata calls during extraction.

The dominant MyPal costs—decompression, hashing, antivirus scanning, and staging
cleanup—remain. Repackaging MyPal beneath `payload` and renaming that directory
would remove its script's full payload copy. The MyPal package recipe is not held
in this repository, so that change must be made where its archive is assembled.
