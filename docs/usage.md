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

Display detailed version and dependency information:

```text
wpm --verbose
wpm --version --verbose
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

Each ZIP package is extracted to `C:\TEMP\<package-name>`. When the package
contains `.wpm/index.csv`, WPM verifies each indexed file's size and BLAKE2b
signature after extraction and fails installation if any indexed file does not
match.
Existing files in that directory may be overwritten. This temporary
installation location is expected to change as installation support develops.

### Remove packages

```text
wpm remove <package...>
```

Removes one or more packages:

```text
wpm remove pkg1 pkg2
```

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
  `test-tc-0005-x86-debug`
