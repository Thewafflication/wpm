# WPM Usage Guide

WPM (Waughtal Package Manager) is a command-line tool for creating, building,
installing, removing, updating, and upgrading packages.

> [!NOTE]
> WPM is under development. The `init` command creates package metadata and
> support files. The other commands currently accept and display their
> arguments, but their package-management behavior is not implemented yet.

## Command syntax

```text
wpm <command> [options]
```

Running `wpm` without a command displays the version and command summary.

## Commands

### Initialize a package

```text
wpm init
```

Initializes a package in the current directory.

WPM prompts for the package name and creates any missing package-support files.
It does not overwrite existing files.

### Build a package

```text
wpm build <source_dir> [output_dir] [--no-index]
```

Builds a package from `source_dir`. When supplied, `output_dir` selects the
output location. Use `--no-index` to skip updating the package index during the
build.

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
wpm install pkg1 pkg2
```

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
filename,crc
```

### `wpmignore.txt`

Lists files and directories that should be excluded when building a package.
The generated file initially excludes `.wpm/`, `*.log`, and `build/`.

### `install.cmd` and `remove.cmd`

Windows command scripts that define how the package is installed and removed.
The generated scripts are templates for package authors to customize.

## Building WPM from source

WPM is a C11 project built with CMake. On Windows, the included presets use
Ninja and the Microsoft C compiler.

```powershell
cmake --preset x64-debug
cmake --build out/build/x64-debug
```

The resulting executable is located under `out/build/x64-debug/wpm/`.
