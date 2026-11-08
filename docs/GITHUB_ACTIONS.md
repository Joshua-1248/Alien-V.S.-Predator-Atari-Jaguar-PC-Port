# Automated Windows, Linux, and macOS build checks

`/.github/workflows/native-builds.yml` runs on every push to `main`, on pull requests, or manually with **Actions → AVP PC Port - Windows, Linux and macOS checks → Run workflow**.

## Build outputs

- **AVP-Windows-x64**: playable Windows x64 `AVP_Jaguar_PC.exe`, generated with MSYS2 UCRT64; includes 51 prototype replacement `.prgb` textures. Windows hardware gameplay verification remains outstanding.
- **AVP-Linux-x64**: playable Linux x64 `LINUX/avp_pc` (GCC) and `LINUX/avp_pc_clang` (Clang) together, plus the 51 replacement textures.
- **AVP-Linux-x64-tar-gz**: the same Linux package as a tarball; use this when preserving executable permissions is important.
- **AVP-macOS-portability-only-NOT-GAME**: two native macOS regression executables for the portable core, not the playable port. **There is no macOS game build yet.** CMake expressly disables `AVP_BUILD_PC` on Apple, and the current audio, video, input and ELF/PE game-state code have no native macOS equivalent.

All artifacts omit the Atari Jaguar retail ROM. Users must supply their own supported `AVP.jag`. Automated compilation does not establish full runtime/platform compatibility. The repository is an unofficial work-in-progress; see `README.md`, `LICENSE.md`, `PROVENANCE.md` and `docs/FEATURE_STATUS.md`.

## GitHub token requirement

Your token needs **Contents: Read and write** and **Workflows: Read and write** permissions (fine-grained), or the `repo` and `workflow` scopes (classic). GitHub rejects a push adding or updating `.github/workflows/*.yml` without workflow permission. This is independent of the Actions job's runtime `contents: read` setting.

The prior `.github/workflows/windows-release.yml` was deliberately excluded and remains ignored by the local `.gitignore`. This update uses the **new** `native-builds.yml` filename so it is trackable. Don't re-enable the old workflow at the same time, or you'll run duplicate jobs.

## Upload (from the existing repository directory)

Extract the ZIP *into the directory containing `.git`*, then run:

```bash
git add -A
git commit -m "Add automated Windows Linux and macOS portability builds"
git push
```

If GitHub rejects the push on workflow permission, update the token's permissions before trying again; don't remove the workflow file. **Never paste a token into project files, command history, or chat.**

## Roadmap to a full macOS game executable

A real macOS target needs native window/display, audio output and gamepad/input handling; changes to the platform guards in CMake and Linux-only headers/paths; replacement of GNU ELF linker-script assumptions; and real macOS gameplay tests. Do not publish the portability-only test files as a game build.
