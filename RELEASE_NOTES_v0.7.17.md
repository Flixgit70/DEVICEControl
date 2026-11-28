# DEVICEControl 0.7.17

## Existing CSV presets and name mapping

- Import the complete name path from existing `branch_1`, `branch_2`, etc.
  columns into the saved mapping identity, independently of custom labels.
- Preserve compatibility with older CSVs using a `tree_path` column and
  numeric-only CSVs. Custom labels are never guessed as full name paths.
- Preserve the saved full name path when exporting a page while the provider
  is disconnected or its old numeric address points at another parameter.
- Keep numeric addresses and custom control labels alongside the name identity.

This release also includes the full-name mapping performance fixes from 0.7.16.

## Packages

GUI and CLI packages for macOS ARM64, Windows x64, Linux x64 and Linux ARM64,
with SHA-256 checksums.

## Verification

Local macOS Release build and both test suites passed. Added coverage for
name identity import from both CSV layouts, custom labels, offline CSV
export/import and numeric-only legacy presets. Platform builds and tests run
in GitHub Actions after the release tag is pushed.
