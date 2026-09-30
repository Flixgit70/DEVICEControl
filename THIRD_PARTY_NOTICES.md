# Third-party components

DEVICEControl uses third-party software. This file is informational and is not
a substitute for reviewing the upstream licence texts before distribution.

## JUCE

- Project: JUCE
- Pinned build version: 8.0.15
- Upstream: https://github.com/juce-framework/JUCE
- Licence terms: see the upstream `LICENSE.md` and JUCE licensing information.

JUCE offers open-source and commercial licensing terms. Depending on how
DEVICEControl is distributed and licensed, a commercial JUCE licence may be
required. Review the current JUCE terms before publishing binaries or source.

## Lawo ember-plus

- Project: Lawo ember-plus
- Pinned SDK version: v1.8.2
- Libraries used: libember and libs101
- Upstream: https://github.com/Lawo/ember-plus
- Upstream repository licence: Boost Software License 1.0 (see `LICENSE.TXT`).

## Build tooling

- CMake >= 3.22
- Git is required only when FetchContent needs to download dependencies; the
  DEVICEControl source repository itself may be hosted in Git, SVN, or another
  version-control system.
