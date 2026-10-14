"""Run with: python3 -m unittest discover -s packaging/macos -v"""

import hashlib
import io
import json
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

from formula import generate


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="anchor-packaging-test-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def archive(self, architecture="arm64", version="0.1.0", metadata_arch=None):
        name = f"anchor-{version}-macos-{architecture}"
        archive = self.directory / f"{name}.tar.gz"
        metadata = json.dumps({
            "version": version,
            "architecture": metadata_arch or architecture,
            "minimum_macos": "14.0",
        }).encode()
        with tarfile.open(archive, "w:gz") as bundle:
            member = tarfile.TarInfo(name + "/release.json")
            member.size = len(metadata)
            bundle.addfile(member, io.BytesIO(metadata))
        return archive

    def test_formula_selects_archives_and_checksums(self):
        arm = self.archive()
        intel = self.archive("x86_64")
        formula = generate([arm, intel], "nekoffski/anchor")
        for archive in (arm, intel):
            self.assertIn(archive.name, formula)
            self.assertIn(hashlib.sha256(archive.read_bytes()).hexdigest(), formula)
        self.assertIn("on_arm do", formula)
        self.assertIn("on_intel do", formula)
        self.assertNotIn("depends_on arch:", formula)
        self.assertIn('run opt_libexec/"anchor-service"', formula)
        self.assertIn('MacOSVersion.new("14.0")', formula)
        self.assertNotIn("require_root true", formula)

    def test_single_architecture_is_restricted(self):
        self.assertIn("depends_on arch: :arm64", generate([self.archive()], "nekoffski/anchor"))

    def test_local_formula_uses_local_archive(self):
        archive = self.archive()
        formula = generate([archive], "nekoffski/anchor", local=True)
        self.assertIn(archive.resolve().as_uri(), formula)
        self.assertNotIn("releases/download", formula)

    def test_rejects_inconsistent_releases(self):
        arm = self.archive()
        with self.assertRaises(ValueError):
            generate([arm, arm], "nekoffski/anchor")
        with self.assertRaises(ValueError):
            generate([arm, self.archive("x86_64", "0.2.0")], "nekoffski/anchor")
        with self.assertRaises(ValueError):
            generate([self.archive(metadata_arch="x86_64")], "nekoffski/anchor")
        with self.assertRaises(ValueError):
            generate([arm], "invalid-repository")

    def test_service_refuses_root_before_creating_directories(self):
        mock_bin = self.directory / "mock-bin"
        mock_bin.mkdir()
        mock_id = mock_bin / "id"
        mock_id.write_text("#!/bin/sh\nprintf '0\\n'\n")
        mock_id.chmod(0o755)
        result = subprocess.run(
            ["/usr/bin/env", f"PATH={mock_bin}:/usr/bin:/bin", "/bin/sh",
             str(Path(__file__).with_name("anchor-service"))],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("without sudo", result.stderr)


if __name__ == "__main__":
    unittest.main()
