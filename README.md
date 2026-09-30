# DEVICEControl 0.7.11

DEVICEControl is a JUCE standalone control application with a protocol-neutral
provider architecture. Ember+ is the current production provider; the codebase
is structured for additional providers such as DirectOut HTTP/JSON.

## Current functionality

The restored 0.7.11 source contains the complete current UI and control logic,
including:

- multi-provider Ember+ browsing and control
- provider-aware pages and mappings
- Button, Encoder and Content controls
- Meter controls (`MeterTile`)
- Matrix browsing/control (`MatrixWindow` and `MatrixPageTile`)
- CSV/project import and export
- include/exclude tree filtering
- editable content and threshold handling
- reconnect / refresh support

## Reproducible dependencies

- JUCE 8.0.15 is pinned and fetched automatically when `JUCE_DIR` is not set.
- Lawo ember-plus v1.8.2 is pinned and fetched automatically when
  `EMBERPLUS_DIR` is not set.
- CMake Presets provide common local and CI entry points.

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

## CI and releases

`.github/workflows/ci.yml` builds and tests normal pushes and pull requests.
`.github/workflows/release.yml` builds tagged releases and attaches packaged
binaries to the matching GitHub Release. Release builds include macOS ARM64, a
separate macOS Sonoma ARM64 build, Windows x64, Linux x64 and Linux ARM64.

See [`README_RELEASES.md`](README_RELEASES.md) and [`docs/CI.md`](docs/CI.md).

## Architecture

- `IProviderBackend` is the protocol-neutral provider contract.
- `ProviderManager` owns/routes configured providers.
- `ProviderFactory` selects concrete provider implementations.
- `providers/ember/EmberProvider` adapts the Ember client.
- `ControlTreeModel` / `ControlTreeBrowser` are protocol-neutral.
- Matrix and meter functionality remains inside the protocol-neutral UI/model
  layers and uses provider-qualified paths.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## App icon

The app icon source is stored as `Data/AppIcon.png` and is wired through
`CMakeLists.txt`, so local and GitHub macOS builds use the same application icon.

## Third-party software / licensing

See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). JUCE has its own
licensing terms; check the applicable JUCE licence before distributing binaries.
