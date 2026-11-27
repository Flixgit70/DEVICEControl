# DEVICEControl 0.7.14

## Highlights

- Added a read-only local CLI for inspecting provider status, Ember+ parameters,
  live values and configured pages.
- Added contextual `?` help, hierarchical Tab completion, editable command
  history, panel row selection and optional live watch/difference output.
- Added a mapping identity choice between Ember path addresses and complete
  name paths. Name mode rebinds controls after scanning the provider tree and
  skips missing or ambiguous matches.
- Added **Deselect All** to clear tree selections, including entries hidden by
  the current filter.
- Updated project files to save the mapping identity mode and stable name paths;
  older projects continue to use path-address matching by default.

## Release packages

The automated release workflow builds the GUI and CLI for macOS ARM64,
Windows x64, Linux x64 and Linux ARM64. Each package
includes the applicable app/binaries, README, third-party notices and CLI guide.
The workflow attaches SHA-256 checksums to the GitHub Release.

## Verification

Local macOS Release build completed. Local regression tests passed: 2/2.
