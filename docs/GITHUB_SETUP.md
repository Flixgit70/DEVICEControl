# Creating the DEVICEControl GitHub repository

The source tree is ready to become the repository root. No generated `build/`
directory or local JUCE checkout should be committed.

## Recommended first repository

Start with a **private** repository unless the intended source/distribution
licence has already been decided, especially because DEVICEControl links JUCE
and JUCE licensing obligations depend on the chosen distribution model.

Suggested repository name: `DEVICEControl`

## Initial push from the project directory

After creating an empty repository on GitHub:

```bash
git init
git add .
git commit -m "Initial DEVICEControl 0.7.0"
git branch -M main
git remote add origin git@github.com:<github-user-or-org>/DEVICEControl.git
git push -u origin main
```

HTTPS remotes work as well if preferred.

The first push to `main` automatically starts `.github/workflows/ci.yml`.

## Recommended GitHub settings

After the first successful CI run:

1. Protect `main`.
2. Require pull requests before merging if several people will contribute.
3. Require the CI checks for Linux, macOS and Windows before merge.
4. Do not commit build output or local dependency checkouts.
5. Use Git tags such as `v0.7.0` for stable source milestones.

## Local development remains unchanged

A developer with a local JUCE checkout can use:

```bash
export JUCE_DIR=/path/to/JUCE
./build_macos.sh
```

Without `JUCE_DIR`, the build downloads pinned JUCE 8.0.15 automatically.
