# DEVICEControl 0.7.16

## Full name path performance

- Replaced full provider snapshots and linear name searches for every control
  refresh/write with a shared index of parameter and matrix identities.
- Rebuild the identity index only when names or topology change. Parameter
  values, matrix crosspoints and directory loading flags do not rebuild it.
- Preserve unique full-name matching per provider; missing, ambiguous and stale
  identities continue to block writes. Incomplete matrix ancestry cannot match.
- Reuse completed discovery within the current connection when toggling mapping
  modes. Queue name scans behind scans already in progress, and defer navigation
  rebuilds while a name scan is collecting the provider tree.
- Limit recursive child searches to the requested subtree.
- Refresh only the active tile/list view and open matrix editors. Ordinary controls and unresolved
  matrices no longer copy the full parameter list during value updates.

## Packages

GUI and CLI packages for macOS ARM64, Windows x64, Linux x64 and Linux ARM64.

## Verification

Local macOS Release build and both test suites passed. Regression coverage
includes duplicate names, provider isolation, address changes, incomplete matrix
ancestry, 2,000 matrices and identity-revision invalidation. Windows runtime
responsiveness with a live provider still needs confirmation on the target PC.
