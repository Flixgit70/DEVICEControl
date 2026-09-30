# DEVICEControl architecture

## Direction

DEVICEControl is a protocol-neutral control surface. Ember+ is the first provider implementation, not the application architecture.

```
UI
  MainComponent / ControlTreeBrowser / Pages / Matrix / Meter controls
        |
Controllers + services
  MappingController / AppSettingsService / CSV / Project
        |
ProviderManager                    provider-qualified paths
        |
IProviderBackend                   protocol-neutral contract
        |
ProviderFactory
   |-- EmberProvider
   |      `-- EmberClient          S101 / Glow / libember
   `-- future DirectOutProvider    HTTP / JSON
```

## Rules

1. UI code must not include libember or libs101 headers.
2. ProviderManager must not include a concrete provider implementation.
3. Concrete provider selection happens only in ProviderFactory.
4. Provider implementations expose local paths through IProviderBackend.
5. ProviderManager is the only layer that adds/removes the stable provider ID.
6. Pages, mapping rows, CSV and project files use provider-qualified paths.
7. Protocol-specific network code stays inside its provider implementation.
8. Model code stays independent of GUI widgets and network transports.

## Current providers

### Ember

`providers/ember/EmberProvider` adapts the existing stable `EmberClient` to `IProviderBackend`. The EmberClient provides the current S101/Glow transport used by the Ember provider.

### Future providers

A DirectOut provider should implement `IProviderBackend` and be registered in `ProviderFactory`. No changes should be required in `MainComponent`, `ControlTreeBrowser`, page controls or Mapping Table for basic browse/read/write behaviour.

## Compatibility wrappers

Legacy filenames (`MultiProviderBackend`, `EmberTreeModel`, `EmberTreeBrowser`, `IControlBackend`) remain as small compatibility aliases/wrappers. New code should use the current provider-neutral names.
