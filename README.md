# DEVICEControl 0.7.0

DEVICEControl is a JUCE standalone control application with a protocol-neutral
provider architecture. The current production provider is Ember+; the codebase
is prepared for additional providers such as DirectOut HTTP/JSON.

## CI-ready repository

Version 0.7.0 removes machine-specific build paths and makes a fresh repository
checkout reproducible on macOS, Linux and Windows.

- JUCE 8.0.15 is pinned and fetched automatically when `JUCE_DIR` is not set.
- Lawo ember-plus v1.8.2 remains pinned and is fetched automatically when
  `EMBERPLUS_DIR` is not set.
- CMake Presets provide identical local and CI entry points.
- GitHub Actions builds and tests Linux x64, macOS Apple Silicon and Windows x64.
- The CI scripts are repository-agnostic and can also run after an SVN/Jenkins
  checkout.
- Build artifacts are uploaded by GitHub Actions for each platform.

See [`docs/CI.md`](docs/CI.md) for all build and CI commands.

## Quick local build

### macOS

```bash
./build_macos.sh
```

### Linux (Debian/Ubuntu)

```bash
./scripts/ci/install_linux_deps.sh
./build_linux.sh
```

### Windows

```bat
build_windows.bat
```

To use an existing local JUCE checkout instead of FetchContent, set `JUCE_DIR`.
The same applies to `EMBERPLUS_DIR`.

## GitHub

The repository contains `.github/workflows/ci.yml`. Once the project is pushed
to GitHub, Actions runs automatically on pushes to `main`, pull requests, and
manual workflow dispatches.

Recommended initial repository branch: `main`.

## Architecture

- `IProviderBackend` is the protocol-neutral provider contract.
- `ProviderManager` owns/routs configured providers.
- `ProviderFactory` selects concrete provider implementations.
- `providers/ember/EmberProvider` adapts the existing Ember client.
- `ControlTreeModel` / `ControlTreeBrowser` are protocol-neutral.
- Pages, Mapping Table, CSV, editable Content, thresholds, filter exclusions,
  multi-provider Ember sessions and Deep Expand are retained.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Third-party software / licensing

See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). In particular, JUCE has
its own licensing terms; depending on how DEVICEControl is distributed, a
commercial JUCE licence may be required.

## Creating the GitHub repository

See [`docs/GITHUB_SETUP.md`](docs/GITHUB_SETUP.md) for the initial `git init`,
first push, and recommended branch-protection setup.
