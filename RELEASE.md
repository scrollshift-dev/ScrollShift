# ScrollShift release and publication handover

## Authority and current state

The development executable currently reports `scrollshift 0.1.2`
(`scrollshift --version`; defined in `include/scrollshift/version.hpp`). The
repository remote is `scrollshift-dev/ScrollShift`. Exact tag, artifact, and
public release conventions follow the evidence in this document and the actual
Git/release state.

## Released versions

- **v0.1.0** — historical initial release at commit `21ce000` (published 2026-08-23); immutable.
- **v0.1.1** — first hardened/publicity-ready release at commit `ac973dd925cf62a76c85c363665a4be8e7533ff6` (tag object `bf701f74d977ca74776cd07041b5c2b5dadb2c13`, published 2026-09-07). Assets: `scrollshift-0.1.1-linux-x86_64.tar.gz`, `scrollshift-0.1.1-linux-aarch64.tar.gz`, `SHA256SUMS`.

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
release is public, `public-download-smoke` in `release.yml` resolves the exact
tagged release through the live public `download.sh`, verifies the
checksum-verified download/artifact version, and confirms live script parity.
The privileged public `install.sh` lifecycle is a separate disposable-host gate.

## Website publication

The ScrollShift website source is a separate repository
(`scrollshift-dev.github.io`) on its authoritative `stage` branch. Its nested
`public/` is a separate generated Git checkout on `main`, deployed by GitHub
Pages. For publication checkpoints, commit the rebuilt/generated `public/`
checkout on `main` first, then the corresponding authoritative source changes
on `stage`, and verify both trees are clean.

The canonical `packaging/install.sh` in this repository is served byte-for-byte
as `https://scrollshift.dev/install.sh`. When the installer changes,
copy it to the website root `install.sh` (and compatibility alias `install`) and commit the generated public script copies
so `public-download-smoke` keeps passing.

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
   aarch64, built with `SCROLLSHIFT_WARNINGS_AS_ERRORS=ON`), `installer-preflight`,
   and `website-parity` (public website scripts byte-identical to `packaging/`)
   must all succeed before the GitHub release is created; the `publish` job
   checks out the repository and verifies the candidate asset set with
   `scripts/verify_release.sh` (both architecture archives plus a structurally
   valid `SHA256SUMS` with exactly one matching entry per archive) before
   creating the release. A rerun against an already-existing release is accepted
   only when two layers hold: the complete published GitHub asset-name set is
   exactly the expected public set (both archives and the published
   `SHA256SUMS`; any name outside the expected three is rejected as an
   unexpected asset via `verify_release.sh --published` — GitHub source-code
   downloads are not release assets and never appear in the asset list), and
   the three assets' contents satisfy `verify_release.sh` with the published
   manifest byte-identical to the candidate manifest. Any missing, extra or
   inconsistent asset fails loudly and is never auto-repaired.
   After publication, require `public-download-smoke` to pass; this verifies the
   live public download path (resolves the tag, enforces the published
   checksum, artifact version) and live script parity. The privileged public
   `install.sh`/service lifecycle remains a disposable-systemd-host gate that
   must be exercised separately before claiming the full public installation
   path.
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

## Release gates and evidence

Gates are assessed at the exact release-candidate SHA. They are split into
release-critical deterministic/CI gates (enforced automatically and required
before tagging), public download/install gates, real-hardware evidence already
obtained, and outstanding non-blocking hardware coverage. Anything not actually
exercised is recorded as outstanding rather than assumed or marked passed.

### Release-critical deterministic/CI gates (enforced in CI and at release)

1. `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DSCROLLSHIFT_WARNINGS_AS_ERRORS=ON`, build, and full `ctest` pass (Debug warnings-as-errors plus ASan/UBSan are also expected; the automated suite includes the classifier matrix, service-unit ownership/migration matrix, auto-mode lifecycle, backoff policy, installer checksum failure cases, release verification partial cases, and the workflow checkout ordering regression).
2. `sh -n` and `shellcheck` on all four public scripts; run `tests/installer_checksum_tests.sh` and `tests/release_verify_tests.sh`.
3. Confirm `scrollshift service nonsense` exits 2 and performs no mutation.
4. Verify website source and generated `public/` repositories are clean and all public shell scripts are byte-identical to `packaging/` (enforced pre-publication by the `website-parity` job).

These passed for v0.1.1, and the tag-triggered release workflow enforces them on every release.

### Public download/install gates

- **Public download path** (resolves `latest`, fetches and enforces the published `SHA256SUMS`, verified download/extraction, artifact version): verified for v0.1.1 locally and by the release workflow's `public-download-smoke` job.
- **Public `install.sh` privileged lifecycle** (fresh install, `service install`, default `mode = auto` config creation, managed unit enable/start, `/usr/local/bin/scrollshift --version`), public `update.sh`, and public `uninstall.sh` with configuration preservation (and `--purge` only in a disposable environment): **outstanding**. These require a disposable systemd host that was not available at release time; do not mark them passed until exercised there.

### Real-hardware evidence already obtained

- Automatic conventional-mouse discovery selected the user's intended physical mouse, and ordinary wheel scrolling works with it (exercised by the user before release).
- Most ordinary service/mouse lifecycle behaviour was exercised manually during development.

### Outstanding non-blocking hardware coverage

The following require hardware the user does not own and are deliberately
non-blocking for v0.1.x (to be covered in a later cycle rather than treated as
release blockers):

- two simultaneous physical mice;
- touchscreen isolation;
- unusual composite receivers;
- broader unusual Linux pointing-device combinations.

Do not claim unexercised hardware combinations as passed. The input-grab
privilege boundary and the full systemd lifecycle remain disposable-host items
to exercise at the next release cycle.
