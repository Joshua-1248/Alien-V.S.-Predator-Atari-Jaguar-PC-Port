# Publishing the prepared source with Git and a GitHub token

Target: **https://github.com/Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-PC-Port**

**The maintainer performs this upload locally.** No token is required to unpack or build the project. Do not put a token in this repository, in a command-line URL, or in a screenshot.

## First-time upload from Linux Mint / Ubuntu

Extract the GitHub-ready source archive into a new folder. Open a terminal **inside that extracted folder** (the folder containing `README.md`, `CMakeLists.txt`, `src/`, etc.), then run:

```bash
git init -b main
git config user.name "Joshua-1248"
git config user.email "YOUR_PUBLIC_OR_NOREPLY_GITHUB_EMAIL"
git remote add origin https://github.com/Joshua-1248/Alien-V.S.-Predator-Atari-Jaguar-PC-Port.git

git add .
git status --short
git diff --cached --stat
git diff --cached --name-only | grep -Ei '\.(jag|j64|rom|aaf|wav|zip|exe|dll|so)$' || true

git commit -m "Initial public source import: Atari Jaguar AVP PC port R71"
git push -u origin main
```

**Review `git status --short` before committing.** No ROM, extracted commercial media, personal settings, token or built executable should be staged. If you see any, remove it from the staging area with `git restore --staged PATH`; `.gitignore` does not protect files that are already tracked.

When Git prompts for HTTPS credentials:

- **Username:** `Joshua-1248`
- **Password:** paste your GitHub **personal access token (PAT)**, **not** your GitHub account password. The token is usually not shown as you type or paste.

Use a **fine-grained PAT** limited to this repository with **Contents: Read and write**, if your GitHub account supports it. If your account requires a classic PAT instead, `repo` is the usual scope for repository push access; avoid additional permissions you do not need. Never paste the PAT in chat, commit it, add it to the remote URL, or leave it in a shell script. GitHub permissions and token configuration can change; follow GitHub's current official documentation.

If credentials fail, inspect `git remote -v` to confirm the URL and check token expiration, repository access and allowed scopes. If the remote has new commits that were not part of this prepared archive, **fetch and reconcile them; do not force-push.**

## Future updates

In the same local clone, update only the changed source files, then:

```bash
git status
git add -A
git diff --cached --stat
git commit -m "Describe the verified fix"
git push
```

Do not commit retail ROMs, runtime caches, proprietary SDK/source files, packaged game binaries, local saves, or credentials. The optional 51-panel prototype restoration pack in this update is a documented exception; review its artwork rights before public distribution. For externally distributable binaries, evaluate the licensing and distribution policy separately from the source push. GitHub Actions may run on push; a passing automated workflow does not mean 100% game completeness.

## Rights review before making a public push

- Read `README.md`, `LICENSE.md`, `PROVENANCE.md`, `CREDITS.md`, `THIRD_PARTY_NOTICES.md`.
- Confirm you are comfortable publishing the reconstructed source and understand that an empty or restrictive project license does **not** automatically grant rights in original copyrighted game code.
- Musashi MIT and SoftFloat's own notices are retained. No retail ROM is included. The 51 prototype-derived `.prgb` texture panels are included for the optional enhancement; file conversion does not grant distribution rights.
- If you need a legal determination of whether a particular decompiled game-code portion can be redistributed, get qualified advice or permission from the relevant rights holders.
