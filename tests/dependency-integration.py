"""Exercise minimum-version graphs through real signed package operations."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--wpm", required=True)
args = parser.parse_args()
wpm = str(Path(args.wpm).resolve())

with tempfile.TemporaryDirectory(prefix="wpm-dependencies-") as directory:
    root = Path(directory)
    data, repo = root / "data", root / "repository"
    repo.mkdir()
    marker = root / "order.txt"
    env = dict(os.environ, WPM_DATA_DIR=str(data))
    for key in ("WPM_LOG_FILE", "WPM_LOG_DIR", "WPM_LOG_LEVEL"):
        env.pop(key, None)

    def run(*arguments, ok=True, answer=""):
        result = subprocess.run(
            [wpm, *map(str, arguments)],
            env=env,
            input=answer,
            text=True,
            capture_output=True,
            timeout=30,
        )
        assert (result.returncode == 0) == ok, (
            arguments,
            result.stdout,
            result.stderr,
        )
        return result.stdout + result.stderr

    private, public = root / "private", root / "public"
    run("keygen", private, public)
    run("trust", "add", public)
    rows = []

    def package(
        name, version="1.0.0", dependencies=(), arch="any", signed=True
    ):
        source = root / f"{name}-{version}"
        meta = source / ".wpm"
        meta.mkdir(parents=True, exist_ok=True)
        (meta / "package.txt").write_text(
            f"name={name}\nversion={version}\narch={arch}\n"
            + "".join(f"depends={item}\n" for item in dependencies)
        )
        for phase in ("install", "upgrade", "remove"):
            (meta / f"{phase}.cmd").write_text(
                "@echo off\n"
                f'echo {name}-{version}-{phase}>>"{marker}"\n'
                "exit /b 0\n"
            )
        (source / "payload.txt").write_text("dependency test payload")
        run("build", source, repo, *(["--sign", private] if signed else []))
        archive = repo / f"{name}-{arch}-{version}.zip"
        rows.append(
            dict(name=name, version=version, arch=arch, url=archive.name)
        )
        return archive

    def publish():
        (repo / "index.json").write_text(
            json.dumps(dict(version=1, packages=rows))
        )

    def order():
        return marker.read_text().splitlines() if marker.exists() else []

    package("leaf")
    package("leaf", "1.5.0")
    package("leaf", "9.0.0-dev")
    package("left", dependencies=("leaf>=1.0.0",))
    package("right", dependencies=("leaf>=1.5.0",))
    app = package("app", dependencies=("left>=1.0.0", "right>=1.0.0"))
    package("other", dependencies=("leaf>=1.0.0",))
    publish()
    run("repo", "add", repo, "--priority", "1000")
    # No package script or retained archive is created by a preview/cancel.
    output = run("install", "app", "--offline", "--dry-run")
    assert "leaf" in output and "1.5.0" in output, output
    assert not order()
    run("install", "app", "--offline", "--confirm", answer="n\n")
    assert not order()
    run("install", "app", "other", "--offline", "--yes")
    assert order() == [
        "leaf-1.5.0-install",
        "left-1.0.0-install",
        "right-1.0.0-install",
        "app-1.0.0-install",
        "other-1.0.0-install",
    ], order()
    # Retained compatible dependencies satisfy later ZIP-path installs offline.
    before = order()
    run("install", app, "--offline", "--yes")
    assert order() == before + ["app-1.0.0-install"], order()

    # Resolve upgrades before the requested parent upgrade.
    package("leaf", "2.0.0")
    package("app", "2.0.0", ("leaf>=2.0.0",))
    publish()
    before = order()
    run("upgrade", "app", "--offline", "--yes")
    changes = order()[len(before) :]
    assert changes[-1].startswith("app-2.0.0-"), changes
    assert any(line.startswith("leaf-2.0.0-") for line in changes), changes
    assert not any("9.0.0" in line for line in order()), order()

    package("cycle-a", dependencies=("cycle-b>=1.0.0",))
    package("cycle-b", dependencies=("cycle-a>=1.0.0",))
    package("missing", dependencies=("unavailable>=1.0.0",))
    package("too-new", dependencies=("leaf>=50.0.0",))
    package("blocked", dependencies=("left>=1.0.0", "unavailable>=1.0.0"))
    package("self", dependencies=("self>=1.0.0",))
    package("foreign", arch="arm64")
    package("wrong-arch", dependencies=("foreign>=1.0.0",), arch="x86")
    publish()
    before = order()
    for name, message in (
        ("cycle-a", "cycle"),
        ("missing", "no compatible"),
        ("too-new", "no compatible"),
        ("blocked", "no compatible"),
        ("self", "cycle"),
        ("wrong-arch", "no compatible"),
    ):
        output = run(
            "install",
            name,
            "--offline",
            "--yes",
            *(["--arch", "x86"] if name == "wrong-arch" else []),
            ok=False,
        )
        assert message in output, output
        assert order() == before, (name, order())

    # Upgrade --all must share dependencies with later explicit root items.
    old_parent = package("all-parent")
    package("all-parent", "2.0.0", ("leaf>=2.0.0",))
    publish()
    for mode in ("--yes", "--confirm"):
        data = root / f"upgrade-all-baseline-{mode}"
        env["WPM_DATA_DIR"] = str(data)
        run("trust", "add", public)
        run("repo", "add", repo, "--priority", "1000")
        run("install", old_parent, "--offline", "--yes")
        run("install", repo / "leaf-any-1.0.0.zip", "--offline", "--yes")
        before = order()
        run("upgrade", "--all", "--offline", mode, answer="y\n")
        changes = order()[len(before) :]
        assert sum(line.startswith("leaf-2.0.0-") for line in changes) == 1, (
            changes
        )
        assert changes[-1].startswith("all-parent-2.0.0-"), changes
    before = order()

    # Dependency script failure must prevent parent execution.
    package("fails")
    (root / "fails-1.0.0" / ".wpm" / "install.cmd").write_text(
        "@echo off\nexit /b 23\n"
    )
    run("build", root / "fails-1.0.0", repo, "--sign", private)
    package("needs-failure", dependencies=("fails>=1.0.0",))
    publish()
    run("install", "needs-failure", "--offline", "--yes", ok=False)
    assert order() == before, order()

    # A ZIP parent works on a clean root as well as with retained dependencies.
    data = root / "fresh-data"
    env["WPM_DATA_DIR"] = str(data)
    run("trust", "add", public)
    run("repo", "add", repo, "--priority", "1000")
    before = order()
    run("install", app, "--offline", "--yes")
    assert order()[len(before) :] == [
        "leaf-2.0.0-install",
        "left-1.0.0-install",
        "right-1.0.0-install",
        "app-1.0.0-install",
    ], order()

    # A missing later branch must not run the valid first branch's scripts.
    data = root / "blocked-data"
    env["WPM_DATA_DIR"] = str(data)
    run("trust", "add", public)
    run("repo", "add", repo, "--priority", "1000")
    before = order()
    run("install", "blocked", "--offline", "--yes", ok=False)
    assert order() == before, order()

    # Explicit x86 roots can depend on any; an insufficient x86 candidate
    # must not hide a sufficient any candidate.
    package("portable", "1.0.0", arch="x86")
    package("portable", "2.0.0")
    package("x86-parent", dependencies=("portable>=2.0.0",), arch="x86")
    publish()
    before = order()
    run("install", "x86-parent", "--arch", "x86", "--offline", "--yes")
    assert order()[len(before) :] == [
        "portable-2.0.0-install",
        "x86-parent-1.0.0-install",
    ], order()

    package("pre-parent", dependencies=("leaf>=9.0.0-dev",))
    publish()
    before = order()
    run("install", "pre-parent", "--offline", "--yes", ok=False)
    assert order() == before, order()
    run("config", "set", "prerelease", "true", "--package", "leaf")
    run("install", "pre-parent", "--offline", "--yes")
    assert order()[-1] == "pre-parent-1.0.0-install", order()
    assert any(line.startswith("leaf-9.0.0-dev-") for line in order())

    # An unsigned dependency cannot inherit the parent's signature authority.
    package("unsigned", signed=False)
    package("signed-parent", dependencies=("unsigned>=1.0.0",))
    publish()
    before = order()
    run("install", "signed-parent", "--offline", "--yes", ok=False)
    assert order() == before, order()

    # Reject the 65th selected node before running the otherwise valid chain.
    for number in range(65):
        package(
            f"chain-{number}",
            dependencies=(f"chain-{number + 1}>=1.0.0",) if number < 64 else (),
        )
    # Catalogs are bounded to 96 entries; retain only this chain for the case.
    rows[:] = [row for row in rows if row["name"].startswith("chain-")]
    publish()
    output = run("install", "chain-0", "--offline", "--yes", ok=False)
    assert "exceeds 64" in output, output
    assert order() == before, order()

    # An invalid declaration must fail at package construction, not be ignored.
    source = root / "invalid"
    meta = source / ".wpm"
    meta.mkdir(parents=True)
    for declaration in (
        "",
        "leaf",
        "leaf>=1.0",
        "leaf>=1.0.0,<2.0.0",
        "../leaf>=1.0.0",
        "leaf>=01.0.0",
        "leaf>1.0.0",
    ):
        (meta / "package.txt").write_text(
            f"name=invalid\nversion=1.0.0\narch=any\ndepends={declaration}\n"
        )
        output = run("build", source, repo, "--sign", private, ok=False)
        assert "invalid or oversized dependency" in output, output

print("Dependency integration passed")
