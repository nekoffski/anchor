#!/usr/bin/env python3
"""Generate a Homebrew formula from local macOS release archives."""

import argparse
import hashlib
import json
import re
import tarfile
from pathlib import Path


def generate(archives, repository, local=False):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise ValueError("repository must be owner/name")
    versions = set()
    releases = {}
    for archive in archives:
        match = re.fullmatch(
            r"anchor-([0-9]+\.[0-9]+\.[0-9]+)-macos-(arm64|x86_64)\.tar\.gz",
            archive.name,
        )
        if not match:
            raise ValueError(f"unexpected archive name: {archive.name}")
        version, architecture = match.groups()
        versions.add(version)
        if architecture in releases:
            raise ValueError(f"duplicate architecture: {architecture}")
        with tarfile.open(archive, "r:gz") as bundle:
            metadata_file = bundle.extractfile(
                archive.name.removesuffix(".tar.gz") + "/release.json"
            )
            if metadata_file is None:
                raise ValueError("archive is missing release.json")
            with metadata_file:
                metadata = json.load(metadata_file)
        if (
            metadata.get("version") != version
            or metadata.get("architecture") != architecture
        ):
            raise ValueError("release metadata does not match the archive name")
        minimum = metadata.get("minimum_macos", "")
        if not isinstance(minimum, str) or not re.fullmatch(
            r"[0-9]+\.[0-9]+(?:\.[0-9]+)?", minimum
        ):
            raise ValueError("invalid minimum macOS version")
        releases[architecture] = (
            archive.name,
            hashlib.sha256(archive.read_bytes()).hexdigest(),
            minimum,
            (
                archive.resolve().as_uri()
                if local
                else f"https://github.com/{repository}/releases/download/v{version}/{archive.name}"
            ),
        )
    if len(versions) != 1:
        raise ValueError("archives must all have the same version")
    version = versions.pop()
    lines = [
        "class Anchor < Formula",
        '  desc "Backup CLI and daemon"',
        f'  homepage "https://github.com/{repository}"',
        f'  version "{version}"',
        '  license "MIT"',
        "",
        "  depends_on :macos",
    ]
    if len(releases) == 1:
        arch = "arm64" if "arm64" in releases else "x86_64"
        lines.append(f"  depends_on arch: :{arch}")
    lines += ["", "  on_macos do"]
    for architecture, (_, checksum, _, url) in sorted(releases.items()):
        selector = "on_arm" if architecture == "arm64" else "on_intel"
        lines += [
            f"    {selector} do",
            f'      url "{url}"',
            f'      sha256 "{checksum}"',
            "    end",
        ]
    lines += [
        "  end",
        "",
        "  def install",
    ]
    for architecture, (_, _, minimum, _) in sorted(releases.items()):
        predicate = "arm?" if architecture == "arm64" else "intel?"
        lines += [
            f'    if Hardware::CPU.{predicate} && MacOS.full_version < MacOSVersion.new("{minimum}")',
            f'      odie "This release requires macOS {minimum} or newer"',
            "    end",
        ]
    lines += [
        '    bin.install "bin/anchor-cli", "bin/anchord"',
        '    libexec.install "libexec/anchor-service"',
        '    pkgshare.install "share/anchor/server.conf"',
        "  end",
        "",
        "  service do",
        '    run opt_libexec/"anchor-service"',
        "    keep_alive successful_exit: false",
        "    process_type :background",
        "  end",
        "",
        "  def caveats",
        "    <<~EOS",
        "      Run this service as your user, without sudo:",
        "        brew services start anchor",
        "      Configuration and workspace directories are created at service startup in:",
        "        ~/Library/Application Support/Anchor/",
        "      Logs are appended in ~/Library/Logs/Anchor/.",
        "      server.conf is copied into the user's config directory on first startup",
        "      and passed to anchord as its first argument. Existing files are preserved.",
        "      Backup plan .conf files belong in:",
        "        ~/Library/Application Support/Anchor/config/plans/",
        "      server.conf uses paths workspace_dir and backup_plans_dir.",
        "      Grant anchord Full Disk Access in System Settings if backup sources require it.",
        "      The daemon does not yet load configuration files.",
        "    EOS",
        "  end",
        "",
        "  test do",
        '    system bin/"anchor-cli"',
        '    assert_predicate bin/"anchord", :executable?',
        '    assert_predicate libexec/"anchor-service", :executable?',
        '    assert_predicate pkgshare/"server.conf", :file?',
        "  end",
        "end",
        "",
    ]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archives", type=Path, nargs="+")
    parser.add_argument("--repository", default="nekoffski/anchor")
    parser.add_argument(
        "--local",
        action="store_true",
        help="use local file URLs instead of GitHub releases",
    )
    parser.add_argument("--output", type=Path, default=Path("dist/Formula/anchor.rb"))
    args = parser.parse_args()
    try:
        formula = generate(args.archives, args.repository, local=args.local)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(formula)
        print(args.output)
    except (OSError, ValueError, KeyError, tarfile.TarError) as error:
        parser.exit(1, f"formula: {error}\n")


if __name__ == "__main__":
    main()
