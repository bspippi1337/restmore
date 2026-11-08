# RESTMORE / Changelog

This changelog separates shipped and verified artifacts from platform support that still requires packaging and device testing.

## [v0.1.0-alpha.4](https://github.com/bspippi1337/restmore/releases/tag/v0.1.0-alpha.4) · 2026-11-08

**Native mobile ergonomics follow-up.**

- Android renderer now scales UI units against physical screen dimensions to maintain legible controls on dense portrait phone screens.
- Unified mouse/touch activation semantics for native buttons and inputs.
- Vertical touch-drag scrolling added to scope, route and inspector panels.
- ARM64 Android NativeActivity debug APK versionCode incremented to 4.
- Full native release pipeline again passed Linux, macOS, Windows HTTPS and Android build/package/signature checks; SHA-256 manifests are provided.

**Validation boundary:** Android package and signature are verified in CI; soft-keyboard integration, touchscreen behavior and rendering on a physical device are not yet confirmed. No iOS IPA is included.

## [v0.1.0-alpha.3](https://github.com/bspippi1337/restmore/releases/tag/v0.1.0-alpha.3) · 2026-11-08

**Art-directed native GUI and the first Android NativeActivity test APK.**

- Native raylib GUI refreshed with a unified dark/mint visual system, vector logo, adaptive layout, scoped controls, interactive orbital operation map, route index and source inspector.
- Fixed native compiler conflicts on Windows and raylib, verified native GUI builds on Linux, macOS and Windows.
- Fixed local OpenAPI import to preserve the actual document's SHA-256 fingerprint for operation provenance, with regression coverage.
- Compiled and debug-signed an ARM64 Android NativeActivity APK in CI, bundling an offline OpenAPI fixture and a vector launcher icon.
- New root index entry supports GitHub Pages when the repository uses branch-root deployment; GitHub Pages build and deployment reached a successful state.
- Captured a real native GUI screenshot during the Linux Xvfb visual smoke test.
- Release workflow published desktop archives, the Android debug APK and a combined SHA-256 manifest after successful build/test/package jobs.

**Limits:** Android APK is an offline engineering preview, not physically device-tested. It uses an ephemeral debug signing key. iOS app packaging is still outstanding. Do not mistake the static Pages topology for a live API scan.

## [v0.1.0-alpha.2](https://github.com/bspippi1337/restmore/releases/tag/v0.1.0-alpha.2) · 2026-11-08

**Native C11 preview with full desktop HTTPS support.**

- Windows x64 builds now include the libcurl HTTPS documentation crawler with native Schannel TLS; the corresponding Windows HTTPS CI job passed.
- New strict JSON number lexer rejects malformed or non-RFC 8259 numeric literals such as `NaN`, `Infinity`, `+1`, `01`, `0x10`, `1.` and `2e`.
- Regression tests cover valid and invalid JSON number tokens.
- Release pipeline produced three native desktop archives and `SHA256SUMS.txt`.
- All release build/test/package jobs passed on Linux x86-64, macOS and Windows x64.

### Verify downloads

Download from the versioned release page and compare the ZIP's SHA-256 against `SHA256SUMS.txt`. These builds are unsigned developer previews; no production security certification or notarization is claimed.

## [v0.1.0-alpha.1](https://github.com/bspippi1337/restmore/releases/tag/v0.1.0-alpha.1) · 2026-11-08

**First native downloadable RESTMORE preview.**

- ISO C11 engine, CLI and graphical raylib interface packaged for Linux, macOS and Windows.
- OpenAPI JSON import and structured atlas export.
- Linux/macOS included HTTPS documentation discovery; Windows was an offline-only native preview.
- Automated C regression testing and release SHA-256 manifests.
- A unified visual design system, illustrative HTML/CSS/SVG website and architecture/build docs.

## Known gaps

- Android and iOS **C11 core libraries** compile in dedicated CI workflows, but there are no installable Android APK or iOS IPA assets in these releases.
- GitHub Pages site source lives in `docs/`. Initial activation requires the repository owner to select **Settings > Pages > Build and deployment > GitHub Actions**. The configured deployment workflow cannot enable Pages with its present integration privileges.
- Native GUI runtime smoke testing is currently automated on Linux with Xvfb; device-level interactive validation, accessibility testing, package signing and macOS notarization remain separate release requirements.
- This is an evidence-based documentation mapper, not a fuzzer and not a complete API client or request executor.

## Release workflow

`.github/workflows/release.yml` captures the latest alpha release pipeline. Future tags should be versioned separately and published only after their own build/test matrix passes.
