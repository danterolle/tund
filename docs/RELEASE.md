# TunD — Release checklist

Use this checklist for versioned releases.

## Version bump

Update every versioned surface in one commit:

- `src/app/version.h`
- `gui/pubspec.yaml`
- `web/index.html` release badge and download links

Use a commit like:

```text
chore: release X.Y
```

## Validation

Run the C checks:

```bash
make format-check
make lint
make test
make sanitize
make tools
make peerforge-check
make peerforge-wrong-key-check
make all
```

For GUI changes, run:

```bash
cd gui
flutter analyze
flutter test
```

If available locally, also run the Windows cross-build:

```bash
make windows
```

## Tag and push

Create and push the release tag:

```bash
git tag -a vX.Y -m "vX.Y"
git push origin main
git push origin vX.Y
```

The `Build & Release` workflow builds release assets when a `v*` tag is pushed.

## Website

Website sources live in `web/`.

GitHub Pages is configured to deploy through the `Pages` workflow, which publishes `web/` as the site root. Do not put website source content in a root-level `index.html`.

After changing website files, verify the `Pages` workflow and check:

```text
https://danterolle.github.io/tund/
```
