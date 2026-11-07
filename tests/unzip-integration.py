"""Exercise plain ZIP extraction and failure boundaries with a built WPM."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile


def main():
    executable = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="wpm-unzip-") as temporary:
        root = Path(temporary)
        environment = dict(os.environ, WPM_DATA_DIR=str(root / "data"))

        def run(*arguments, success=True):
            result = subprocess.run(
                [executable, *map(str, arguments)],
                env=environment,
                capture_output=True,
                text=True,
                check=False,
            )
            assert (result.returncode == 0) == success, (
                result.stdout + result.stderr
            )

        archive = root / "plain payload.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
            output.writestr("nested/hello.txt", "hello")
            output.writestr("empty/", "")
        destination = root / "expanded files"
        run("unzip", archive, destination)
        assert (destination / "nested/hello.txt").read_text() == "hello"
        assert (destination / "empty").is_dir()
        run("unzip", archive, destination, success=False)
        assert (destination / "nested/hello.txt").read_text() == "hello"
        run("unzip", archive, success=False)
        run("unzip", archive, root / "extra", "extra", success=False)
        run("unzip", archive, root / "option", "--bogus", success=False)
        run("unzip", root / "missing.zip", root / "missing", success=False)
        run("unzip", archive, root / "dry", "--dry-run", success=False)
        assert not (root / "dry").exists()
        run("help", "unzip")

        for index, name in enumerate(
            [
                "../escaped.txt",
                "..\\escaped.txt",
                "/absolute.txt",
                "C:/drive.txt",
            ]
        ):
            unsafe = root / f"unsafe-{index}.zip"
            with zipfile.ZipFile(unsafe, "w") as output:
                output.writestr(name, "unsafe")
            run("unzip", unsafe, root / f"unsafe-{index}", success=False)
        assert not (root / "escaped.txt").exists()

        link = root / "symlink.zip"
        info = zipfile.ZipInfo("link")
        info.create_system = 3
        info.external_attr = 0o120777 << 16
        with zipfile.ZipFile(link, "w") as output:
            output.writestr(info, "../escaped.txt")
        run("unzip", link, root / "symlink", success=False)

        corrupt = root / "corrupt.zip"
        with zipfile.ZipFile(corrupt, "w", zipfile.ZIP_STORED) as output:
            output.writestr("file.txt", b"original-payload")
        corrupt.write_bytes(
            corrupt.read_bytes().replace(
                b"original-payload", b"modified-payload"
            )
        )
        run("unzip", corrupt, root / "corrupt", success=False)
        assert not (root / "corrupt/file.txt").exists()
    print("Unzip integration checks passed.")


if __name__ == "__main__":
    main()
