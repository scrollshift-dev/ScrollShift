# ScrollShift release and publication handover

## Authority and current state

The development executable currently reports `scrollshift 0.1.0`
(`scrollshift --version`; defined in `include/scrollshift/version.hpp`). The
repository remote is `scrollshift-dev/ScrollShift`. Exact tag, artifact, and
public release conventions follow the evidence in this document and the actual
Git/release state.

ScrollShift is a Linux-only input daemon (evdev/uinput). Cross-platform
compatibility is not a release concern; cross-distribution/desktop compatibility
is tracked separately in `docs/COMPATIBILITY.md`.

A website content checkpoint and executable version are distinct identities. Do
not synchronize version numbers mechanically.

## Checkpoint versus release

```text
validated checkpoint
    coherent development baseline, not public by implication

release candidate
    validated checkpoint undergoing packaging/publication checks

release
    deliberately published artifact after approval
```

## Release-candidate validation

Proportionately include:

1. Clean release build (`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`) with
   g++ and clang++.
2. Full test set (`ctest --test-dir build --output-on-failure`: CLI smoke,
   input/velocity/transform property tests, the fixed-seed 100,000-packet
   randomized transform campaign, config parsing, daemon lifecycle, uinput
   experiment invariants).
3. The retained checkpoint evidence in `docs/` where relevant.
4. Review the website (`scrollshift-dev.github.io`): complete any
   release-targeted items, build the website with the intended Nift binary, and
   verify the generated site.
5. Reconcile README, docs, website, AI context, release notes, decisions, and
   the roadmap.
6. Build the actual archive layouts the release workflow will publish, extract
   them freshly, and run the executable (`scrollshift --version`) plus
   `--help`.
7. Inspect repository state for generated/debug residue.

Repository tests passing does not prove a release archive is usable. After the
release is public, `installer-public-smoke` in `release.yml` installs the exact
tagged release through the live website installer on Linux and verifies the
installed binary version.

## Website publication

The ScrollShift website source is a separate repository
(`scrollshift-dev.github.io`) on its authoritative `stage` branch. Its nested
`public/` is a separate generated Git checkout on `main`, deployed by GitHub
Pages. For publication checkpoints, commit the rebuilt/generated `public/`
checkout on `main` first, then the corresponding authoritative source changes
on `stage`, and verify both trees are clean.

The canonical `packaging/install.sh` in this repository is served byte-for-byte
as `https://scrollshift-dev.github.io/install`. When the installer changes,
copy it to the website root `install` and commit the generated `public/install`
so `installer-public-smoke` keeps passing.

## Version and notes

Behavioral changes and correctness fixes may justify version/release-note
changes. Follow established Git/release evidence and ask before assigning a
public release version.

Immediately after a public release, advance the executable identity in
`include/scrollshift/version.hpp` (and the CMake project version) to the next
development version before further development. Update the website and release
notes as part of the same post-release checkpoint.

## Release report

Record exact source/suite/site identities, commands, outcomes, environment
where material, package contents, known limitations, and publication status.
Separate facts from interpretation. For packaged releases, also record artifact
checksums where applicable.

## Step-by-step release guide

Use this order for a normal `X.Y.Z` production release. Stop at any failed gate,
fix the problem before tagging where possible, and retain exact evidence.

### 1. Prepare the release candidate

1. Start from the intended clean `main` commit and review unrelated working-tree
   changes before touching release files.
2. Choose `X.Y.Z` with approval. Update the version in
   `include/scrollshift/version.hpp` and the CMake project version.
3. Verify `scrollshift --version` and `scrollshift --help` are release-ready.
4. Complete the release-candidate validation above, including the full test
   set and residue inspection.
5. Build the same archive layouts the workflow will publish; extract and test
   the contained executable.
6. Commit and push all approved release-preparation changes. Recheck that
   `main` and the intended release commit are exactly the state validated.

### 2. Create the GitHub release

1. Obtain explicit approval for the public release action.
2. Create the approved annotated `vX.Y.Z` tag at the validated commit and push
   it to `scrollshift-dev/ScrollShift`.
3. Watch `.github/workflows/release.yml`. Both Linux artifact jobs (x86-64,
   aarch64) plus `installer-preflight` must succeed before the GitHub release is
   created. After publication, require `installer-public-smoke` to pass; this
   proves the live website installer matches the tag, verifies the release
   checksum, and installs the tagged release.
4. Confirm the release contains exactly the expected Linux archives and
   `SHA256SUMS`, and that each archive name and embedded executable version
   match `X.Y.Z`.
5. Download at least the checksums and representative archives from the public
   release URL, verify them independently, and record the release URL, tag
   commit, workflow run and final checksums.
6. From this point, treat every published asset as immutable. Never replace an
   archive at the same URL. A workflow rerun should leave an existing release
   untouched.

### 3. Close the release

1. Record the exact tag/commit, GitHub release and workflow URLs, final
   checksums, installation tests and known limitations in this handover.
2. Update the website install/download instructions only with availability that
   has been confirmed from the public release.
