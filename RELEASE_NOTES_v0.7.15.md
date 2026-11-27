# DEVICEControl 0.7.15

## Release build fixes

- Fixed a Linux ARM64 compilation error in CLI command parsing by explicitly
  keeping the quote delimiter as a JUCE character type.
- Removed the separate macOS Sonoma package and its backfill workflow.
- Updated the normal macOS CI job to the macOS 15 runner.

## Features included from 0.7.14

- Read-only local CLI for provider status, Ember+ parameters, live values and
  configured pages, with contextual help, Tab completion and live watch output.
- Mapping identity choice between numeric Ember path addresses and complete
  name paths. Name mode rebinds controls after scanning and skips missing or
  ambiguous matches.
- Deselect All clears tree selections, including entries hidden by filters.
- Project files retain mapping identity mode and stable name paths; older
  projects default to path-address matching.

## Packages

The release workflow builds GUI and CLI packages for macOS ARM64, Windows x64,
Linux x64 and Linux ARM64, including documentation and SHA-256 checksums.

## Verification

Local macOS Release build and both regression test suites passed. Linux ARM64
validation runs in GitHub Actions.
