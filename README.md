# Anchor

## Installation

### macOS: Homebrew user service

The Homebrew package is named `anchor`. Its daemon runs as the user who starts
the service (for example, `kaper`), with that user's file permissions. It does
not use a separate `anchor` account. Start it without `sudo`; the service
launcher refuses to run as root.

The package provides `anchor-cli` and `anchord` in Homebrew's `bin` directory
and a service launcher in the formula's `libexec` directory. Homebrew generates
the LaunchAgent from the formula's `service` block. The service starts at login
and can run while the screen is locked; it does not run before login or after
logout.

At service startup the launcher prepares these private user directories:

| Purpose | Path |
| --- | --- |
| Configuration | `~/Library/Application Support/Anchor/config/` |
| Backup plans (`.conf` files) | `~/Library/Application Support/Anchor/config/plans/` |
| Persistent workspace | `~/Library/Application Support/Anchor/workspace/` |
| Cache | `~/Library/Caches/Anchor/` |
| Logs | `~/Library/Logs/Anchor/` |

The daemon runs with the workspace as its working directory. Output is appended
to `anchord.log` and `anchord-error.log`; log rotation is not configured yet.
Directories and logs are created with private permissions. Existing user data
is preserved on restart, upgrade, and uninstall. An INI-style `server.conf` ships
in the formula's `share/anchor` directory and is copied to
`~/Library/Application Support/Anchor/config/server.conf` on first service
startup. Existing configurations are preserved. The launcher passes its
absolute path as the first argument to `anchord`; configuration parsing is
not implemented yet.

The packaged configuration contains:

```conf
paths = {
    workspace_dir = ../workspace
    backup_plans_dir = plans
}
```

Relative paths are intended to resolve against the directory containing
`server.conf`, not the daemon's working directory. These defaults identify the
workspace and backup-plan directories above without embedding a username.
New installations receive this file automatically. Existing files, including
empty configurations from earlier installations, are preserved.

`workspace_dir` holds durable state such as SQLite databases and other
application files. `backup_plans_dir` holds the `.conf` files describing backup
plans. Backup data goes to the destinations specified by those plans, separately
from Anchor's internal workspace.

When upgrading an earlier installation, the launcher creates `config/plans`
but preserves existing configuration and files. If `server.conf` still uses
`backup_config_dir`, rename that setting to `backup_plans_dir`. Use `plans` for
the new directory and move existing plan files there, or retain the old directory
as the setting's value. Packaging does not automatically migrate existing plans.

For protected backup sources, grant the daemon Full Disk Access in System
Settings > Privacy & Security. This does not grant access beyond the service
user's ordinary file permissions.

#### Build a release archive

For now, macOS releases target the maintainer's current Apple Silicon machine.
Older macOS versions and Intel builds are not part of the current validation.
The formula checks the minimum OS version recorded in the binaries.

On a macOS build machine, after `make bootstrap`:

```sh
make package-macos
```

Alternatively, package an existing Release build without rebuilding it:

```sh
python3 packaging/macos/package.py --build-dir build/Release
```

This creates `dist/anchor-<version>-macos-<architecture>.tar.gz` and a SHA-256
checksum file. The archive includes both binaries, the launcher, `server.conf`,
the license, and release metadata. Packaging rejects Debug builds, mismatched binary
architectures, and dependencies on unbundled non-system dynamic libraries.
The version comes from CMake; the architecture and minimum macOS version come
from the installed binaries. Build separate archives on Apple Silicon and
Intel to support both. Only the current architecture is built by this command.
Signing and notarization are not automated yet.

#### Generate the formula

Pass the archives for a single release to the formula generator. For example,
for an Apple Silicon-only release:

```sh
python3 packaging/macos/formula.py dist/anchor-0.1.0-macos-arm64.tar.gz
```

Add the matching Intel archive as a second argument to support both platforms.
The generator writes `dist/Formula/anchor.rb`, calculates checksums, restricts
installation to available architectures, and checks the binary's minimum
macOS version during installation. URLs point to the repository's GitHub
Release named `v<version>`; override the repository with `--repository owner/name`.

Publish the exact archives as GitHub Release assets and copy the generated
formula into `Formula/anchor.rb` in your Homebrew tap repository. These scripts
only create local files; they do not publish releases or register a live service.

Once the release and tap are published:

```sh
brew install <owner>/<tap>/anchor
brew services start <owner>/<tap>/anchor
brew services info <owner>/<tap>/anchor
brew services restart <owner>/<tap>/anchor
brew services stop <owner>/<tap>/anchor
```

#### Install locally without publishing

Build the archive and generate a local formula, then put it in a local Git tap:

```sh
make package-macos
python3 packaging/macos/formula.py dist/anchor-0.1.0-macos-arm64.tar.gz \
    --local --output dist/homebrew-anchor/Formula/anchor.rb
git -C dist/homebrew-anchor init
git -C dist/homebrew-anchor add Formula/anchor.rb
git -C dist/homebrew-anchor commit -m "Package Anchor locally"
brew tap local/anchor "$PWD/dist/homebrew-anchor"
brew install local/anchor/anchor
brew services start local/anchor/anchor
```

Keep the referenced archive on disk for future reinstallations. For a local
rebuild of the same version, rebuild the archive, regenerate the local formula,
and commit the updated formula in `dist/homebrew-anchor`. Then:

```sh
brew services stop local/anchor/anchor
brew update
brew fetch --force local/anchor/anchor
brew reinstall local/anchor/anchor
brew services start local/anchor/anchor
```

Use `brew upgrade local/anchor/anchor` instead of `reinstall` when the formula
has a newer version. `reinstall` replaces the installed package even when the
version is unchanged. Check status with `brew services info local/anchor/anchor`.
For uninstall, stop the service and run `brew uninstall local/anchor/anchor`.

For removal, stop the service first, then run
`brew uninstall <owner>/<tap>/anchor`. User data remains in the Library paths.
After an upgrade, restart the service to load the new daemon binary.

Validate the packaging scripts with:

```sh
python3 -m unittest discover -s packaging/macos -v
```

### CMake installation and Linux service draft

CMake installs `anchor-cli` and `anchord` into the install prefix's `bin`
directory. On Linux it also installs `anchord.service` into
`lib/systemd/system` under that prefix. Override `ANCHOR_SYSTEMD_UNIT_DIR`
at configure time if the distribution uses a different unit directory.

For a Linux package, configure with `-DCMAKE_INSTALL_PREFIX=/usr`. The service
records the configured binary path, so choose the prefix during configuration,
before building and staging:

```sh
cmake --preset conan-release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build --preset conan-release
DESTDIR="$PWD/dist/staging" cmake --install build/Release
```

`DESTDIR` stages files without installing them onto the build machine. It does
not create service accounts, start services, or create runtime directories.

The earlier Linux system-service draft requires an `anchor` system account and
group; this is separate from the macOS user-service model above. It runs in
the foreground with `/var/lib/anchor` as its working directory. systemd creates
and owns `/var/lib/anchor`, `/var/cache/anchor`, and `/run/anchor` for the service.
Logs go to the journal (`journalctl -u anchord`). Persistent data must survive
package upgrades and normal removal.

Linux package-specific account creation, service lifecycle hooks, and
configuration installation are not implemented yet.
