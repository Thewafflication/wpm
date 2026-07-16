# WPM Usage Guide

WPM (Waughtal Package Manager) is a command-line tool for creating, building,
installing, removing, updating, and upgrading packages.

> [!NOTE]
> WPM is under development. Package removal and upgrades are not implemented
> yet.

## Command syntax

```text
wpm <command> [options]
```

Running `wpm` without a command displays the version and command summary.

Display WPM's version:

```text
wpm --version
```

`--version` includes the WPM version plus the bundled `miniz` and `libsodium`
versions and commits:

```text
wpm --version
```

Use `--verbose` with a command to show detailed file-operation progress. It
may appear before or after the command:

```text
wpm --verbose build ./my_project ./dist
wpm build ./my_project ./dist --verbose
wpm --verbose install package.zip
```

## Commands

### Initialize a package

```text
wpm init [package_name]
```

Initializes a package in the current directory and creates the `.wpm`
directory when needed.

When no package name is supplied, WPM uses the current directory name. Provide
a package name to override it. Package names may contain letters, numbers,
`.`, `-`, and `_`.

```text
wpm init
wpm init my-package
```

WPM creates any missing package-support files. It is safe to rerun and does
not overwrite existing files.

### Build a package

```text
wpm build <source_dir> [output_dir] [--no-index]
```

Recursively compresses the contents of `source_dir` into a ZIP package. The
package is named from `.wpm/package.txt` metadata:
`<name>-<version>-<arch>.zip` for release packages and
`<name>-<version>-<arch>-debug.zip` when `debug=true`. It is written to the
current directory unless `output_dir` is supplied.

By default, WPM updates `.wpm/index.csv` with file sizes and BLAKE2b file
signatures before creating the ZIP archive. Use `--no-index` to skip updating
the package index during the build.

Examples:

```text
wpm build ./my_project
wpm build ./my_project ./dist --no-index
```

### Install packages

```text
wpm install <package...>
```

Installs one or more packages:

```text
wpm install ./dist/my_project.zip
wpm install ./dist/pkg1.zip ./dist/pkg2.zip
```

For each ZIP package, WPM extracts into
`%ProgramData%\WPM\temp\<archive-name>`, where `<archive-name>` is the ZIP
file name without `.zip`. When the package contains `.wpm/index.csv`, WPM
verifies each indexed file's size and BLAKE2b signature before it runs the
package's `.wpm\install.cmd`. Installation fails if extraction, verification,
or the install script fails.

After a successful installation, WPM saves a copy of the ZIP archive as
`%ProgramData%\WPM\packages\<archive-name>.zip`. The staging directory is
removed after every installation attempt. The install script controls where
the package deploys software; WPM does not impose a deployment location.

### Remove packages

```text
wpm remove <package...>
```

Removes one or more packages:

```text
wpm remove my-project-1.2.3-any
wpm remove my-project-1.2.3-any.zip
```

The argument identifies a retained archive in
`%ProgramData%\WPM\packages`; use the archive name to remove one exact package
version. WPM stages and verifies that archive, runs `.wpm\remove.cmd`, then
deletes the retained archive only if removal succeeds. The staging directory
is removed after every attempt.

### Update the package index

```text
wpm update
```

Updates the package index.

### Upgrade packages

```text
wpm upgrade <package...>
```

Upgrades one or more packages:

```text
wpm upgrade pkg1 pkg2
```

## Package layout

The `wpm init` command creates the following files inside the package's `.wpm`
directory:

```text
.wpm/
|-- package.txt
|-- index.csv
|-- wpmignore.txt
|-- install.cmd
`-- remove.cmd
```

### `package.txt`

Contains package metadata:

```ini
name=my-package
version=0.1.0
arch=any
debug=false
description=
license=
build_date=
repo=
homepage=
maintainer=
```

### `index.csv`

Tracks packaged files and their checksums:

```csv
filename,size,hash,algorithm
```

### `wpmignore.txt`

Lists files and directories that should not be indexed when building a
package. The generated file initially excludes `.wpm/`, `*.log`, and `build/`.
WPM still indexes `.wpm/package.txt`, `.wpm/install.cmd`, `.wpm/remove.cmd`,
and `.wpm/wpmignore.txt`; `.wpm/index.csv` is not indexed.

### `install.cmd` and `remove.cmd`

Windows command scripts that define how the package is installed and removed.
The generated scripts are templates for package authors to customize.

## Installing WPM itself

`setup.cmd` installs a built `wpm.exe` into `%ProgramFiles%\WPM` and creates
WPM's mutable data directory at `%ProgramData%\WPM` by default:

```text
setup.cmd bin\x86\Debug\wpm.exe
```

`remove.cmd` removes that installation. Both scripts accept the
`WPM_INSTALL_DIR` and `WPM_DATA_DIR` environment variables as explicit
directory overrides for automation and testing.

## Building WPM from source

WPM is a C11 project built with CMake. On Windows, the included presets use
Visual Studio 2022 and the Microsoft C compiler.

```powershell
cmake --preset x86-debug
cmake --build out/build/x86-debug --config Debug
```

The resulting executable is located under `bin/x86/Debug/`.

WPM's displayed version is generated from Git during each build. An exact Git
tag, such as `1.0.0`, becomes the version. Otherwise, WPM displays the short
commit hash and appends `-dirty` when tracked files have uncommitted changes.

Pushing a Git tag runs the Release workflow. After the verification suite
passes, the x86 Debug WPM executable packages the Release executables for all
architectures and publishes `wpm-x86-<version>.zip`, `wpm-x64-<version>.zip`,
and `wpm-arm64-<version>.zip` to the GitHub Release for that tag.

## Running tests

The checked-in test cases exercise version reporting, package initialization,
ZIP builds, ZIP installation, and package index signature verification against
the freshly built executable:

```powershell
cmake --build out/build/x86-debug --config Debug --target check
```

The suite creates uniquely named temporary files and removes them when it
finishes. The x86 debug preset also writes LaTeX execution evidence and test
reports to `bin/x86/Debug/`, so the executable, debug symbols, and reports can
be uploaded as one build artifact.

Generate only the LaTeX reports without running the final CTest pass:

```powershell
cmake --build out/build/x86-debug --config Debug --target test-reports
```

Build WPM's distributable ZIP package after generating the reports:

```powershell
cmake --build --preset package-x86-debug
```

The package is written to `bin/x86/Debug/packages/`. It contains
`wpm.exe`, `setup.cmd`, `remove.cmd`, WPM package metadata, and the
generated TeX reports with their execution evidence. It also includes the
checked-in Markdown documentation, `README.md`, `LICENSE.txt`, and
`THIRD_PARTY_NOTICES.md`.

The optional `test-report-pdfs` target requires a local `pdflatex`
installation and is not required for the standard test artifact.

### Visual Studio CMake presets

Visual Studio exposes the checked-in CMake presets in its configure, build,
and test dropdowns.

Useful local presets:

- Configure: `x86-debug`
- Configure with report targets enabled: `x86-debug-reports`
- Build executable: `build-x86-debug`
- Run tests and build LaTeX/PDF reports: `verify-x86-debug`
- Test all cases through CTest: `test-x86-debug`
- Test one case through CTest: `test-tc-0001-x86-debug` through
  `test-tc-0007-x86-debug`
