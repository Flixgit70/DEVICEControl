# DEVICEControl 0.7.18

## Numeric browsing, name-based panel assignment

- Removed automatic recursive provider scans from full-name mapping. Changing
  mapping mode no longer starts collection of the complete device tree.
- Reconnect restores mapped controls through ordinary numeric GetDirectory
  requests. In name mode, only the saved numeric ancestor directories are also
  read to recover the complete names; unrelated subtrees are not collected.
- Panel assignment/remapping uses the names in already loaded metadata. A
  missing or ambiguous name stays unresolved until its branch has been loaded.
- Panel values and writes continue to use the resolved numeric address, with
  cached identity validation. Manual subtree collection remains available.

Includes the CSV compatibility changes from 0.7.17 and the UI index/cache
optimizations from 0.7.16.

## Verification

Local macOS Release build and both test suites passed. Added regression checks
for numeric ancestor routes, shared-directory deduplication and excluding
unrelated branches. Windows runtime behaviour with the live device remains to
be confirmed after the GitHub build.
