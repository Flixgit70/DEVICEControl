# DEVICEControl release process

## Current release candidate

- Version: `0.7.14`
- Tag: `v0.7.14`
- Release notes: [`RELEASE_NOTES_v0.7.14.md`](RELEASE_NOTES_v0.7.14.md)

The version in `CMakeLists.txt` is authoritative. The release workflow checks
that a pushed tag matches it before building.

## Prepare and verify locally

```bash
git status --short
cmake --preset macos-release
cmake --build --preset macos-release --parallel
ctest --preset macos-release --output-on-failure
```

Review the changes and stage only project files intended for the release. Do not
stage local project spreadsheets, `.emberremote` files, or generated `build/`
outputs.

## Publish through GitHub Actions

After the release commit is on `main`, create and push the matching tag:

```bash
git tag -a v0.7.14 -m "DEVICEControl 0.7.14"
git push origin main
git push origin v0.7.14
```

Pushing the tag starts `.github/workflows/release.yml`. It verifies the version,
builds and tests the release on macOS ARM64, Windows x64, Linux x64 and
Linux ARM64, packages the GUI and CLI, calculates SHA-256
checksums, and creates the GitHub Release. For `v0.7.14`, the workflow uses
`RELEASE_NOTES_v0.7.14.md`; other tags use GitHub-generated notes unless a
matching versioned notes file is added.

The resulting release includes ZIP packages and `SHA256SUMS.txt`. Build
artifacts belong on the GitHub Release, not in the source repository.

## Before pushing the tag

- Confirm the intended release commit is on `main`.
- Confirm the tag version matches `CMakeLists.txt`.
- Review the versioned release notes.
- Check the GitHub Actions run for all four build targets and both test suites.
- Confirm the GitHub Release contains each ZIP and `SHA256SUMS.txt`.

The workflow publishes automatically when the tag is pushed. Do not push the
tag until the release commit and notes have been reviewed.
