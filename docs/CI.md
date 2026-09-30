# DEVICEControl CI / build system

DEVICEControl 0.7.0 is repository-agnostic. GitHub Actions, a local SVN/Jenkins
checkout, or a normal developer workspace all invoke the same CMake presets.

## Pinned dependencies

- JUCE: `8.0.15`
- Lawo ember-plus SDK: `v1.8.2`
- CMake: `>= 3.22`
- C++ standard: C++17 for DEVICEControl; the legacy Lawo implementation targets
  are compiled as C++14 because ember-plus v1.8.2 still uses `std::auto_ptr`.

No absolute developer-machine path is required. By default CMake downloads the
pinned dependencies with FetchContent.

For local/offline development you may override either dependency:

```bash
export JUCE_DIR=/path/to/JUCE
export EMBERPLUS_DIR=/path/to/ember-plus
```

On Windows, set the same environment variables before configuring.

## Local macOS

```bash
./build_macos.sh
```

or directly:

```bash
cmake --preset macos-release
cmake --build --preset macos-release --parallel
ctest --preset macos-release
```

## Local Linux

Install build dependencies once on Debian/Ubuntu:

```bash
./scripts/ci/install_linux_deps.sh
```

Then:

```bash
./build_linux.sh
```

## Local Windows

From a Visual Studio developer-capable shell or normal terminal with Visual
Studio 2022 installed:

```bat
build_windows.bat
```

## GitHub Actions

`.github/workflows/ci.yml` runs configure, build and tests for:

- Ubuntu 24.04 x64
- macOS 14 Apple Silicon
- Windows Server 2022 x64

Each job uploads the JUCE artifacts for inspection/download after the build.
The workflow runs on pushes to `main`, pull requests and manual dispatches.

## SVN / Jenkins / local CI

Nothing in the build scripts requires Git. An SVN-based CI job can simply
checkout/update the working copy and call:

Linux/macOS:

```bash
./scripts/ci/build.sh linux-release
# or
./scripts/ci/build.sh macos-release
```

Windows PowerShell:

```powershell
./scripts/ci/build.ps1 windows-release
```

This keeps the CI logic independent from the repository server.

## Fresh-clone reproducibility check

A CI worker should not need a preinstalled JUCE tree. If `JUCE_DIR` and
`EMBERPLUS_DIR` are unset, configuration must report that it is fetching the
pinned versions and then build/tests should complete using only the checked-out
DEVICEControl sources plus those pinned upstream dependencies.
