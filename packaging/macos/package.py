#!/usr/bin/env python3
"""Package an existing Release build into a relocatable macOS binary archive."""

import argparse
import hashlib
import json
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def package(build_dir, output_dir):
    if platform.system() != "Darwin":
        raise ValueError("macOS archives must be packaged on macOS")
    cache = (build_dir / "CMakeCache.txt").read_text()
    if not re.search(r"^CMAKE_BUILD_TYPE:STRING=Release$", cache, re.M):
        raise ValueError("package a Release build (run make build-release first)")
    version = re.search(r"^CMAKE_PROJECT_VERSION:STATIC=([0-9.]+)$", cache, re.M)
    if not version:
        raise ValueError("cannot determine the project version from CMakeCache.txt")
    version = version.group(1)

    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="anchor-package-") as temporary:
        staging = Path(temporary)
        subprocess.run(
            ["cmake", "--install", str(build_dir), "--prefix", str(staging)],
            check=True,
        )
        architecture = None
        minimum_versions = []
        for name in ("anchor-cli", "anchord"):
            binary = staging / "bin" / name
            detected = subprocess.check_output(
                ["lipo", "-archs", str(binary)], text=True
            ).strip()
            if detected not in ("arm64", "x86_64"):
                raise ValueError(f"unsupported binary architecture: {detected}")
            if architecture is not None and detected != architecture:
                raise ValueError("CLI and daemon architectures differ")
            architecture = detected
            load_commands = subprocess.check_output(
                ["otool", "-l", str(binary)], text=True
            )
            minimum = re.search(r"^\s*minos ([0-9.]+)$", load_commands, re.M)
            if not minimum:
                raise ValueError(f"cannot determine the minimum macOS version for {name}")
            minimum_versions.append(tuple(int(part) for part in minimum.group(1).split(".")))
            dependencies = subprocess.check_output(
                ["otool", "-L", str(binary)], text=True
            ).splitlines()[1:]
            for dependency in dependencies:
                path = dependency.strip().split(" (", 1)[0]
                if not path.startswith(("/usr/lib/", "/System/Library/")):
                    raise ValueError(f"{name} depends on an unbundled library: {path}")

        (staging / "libexec").mkdir()
        launcher = staging / "libexec" / "anchor-service"
        shutil.copyfile(ROOT / "packaging/macos/anchor-service", launcher)
        launcher.chmod(0o755)
        shutil.copyfile(ROOT / "LICENSE", staging / "LICENSE")
        (staging / "release.json").write_text(json.dumps({
            "version": version,
            "architecture": architecture,
            "minimum_macos": ".".join(str(part) for part in max(minimum_versions)),
        }, indent=2) + "\n")
        name = f"anchor-{version}-macos-{architecture}"
        archive = output_dir / f"{name}.tar.gz"
        with tarfile.open(archive, "w:gz") as bundle:
            bundle.add(staging, arcname=name)
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(archive.suffix + ".sha256").write_text(
        f"{checksum}  {archive.name}\n"
    )
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/Release")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    try:
        print(package(args.build_dir.resolve(), args.output_dir.resolve()))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"package: {error}\n")


if __name__ == "__main__":
    main()
