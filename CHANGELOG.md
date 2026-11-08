# RESTMORE / Changelog

This changelog separates shipped and verified artifacts from platform support that still requires packaging and device testing.

## [v0.1.0-alpha.6](https://github.com/bspippi1337/restmore/releases/tag/v0.1.0-alpha.6) · 2026-11-08

**Verification Edition / GitHub first, human authorization next.**

- Native GUI's single-domain **MAP / REVIEW GITHUB** action opens a GitHub repository **search** before API verification; search results are not proof of ownership.
- Separate **AUTHORIZE & VERIFY GET** requires a manual click for the reviewed domain. Editing the domain invalidates authorization.
- Offline C11 planner uses only existing atlas operations with literal, non-parameterized GET paths and a declared OpenAPI 200/204 response, capped at five checks.
- The authorization button stays disabled until the catalog can be bound to the selected host, including a public root origin in OpenAPI `servers[].url`.
- HTTPS verifier checks robots rules first and enforces public-only socket validation, no redirects/proxies, per-request timeout, 128 KiB response limit, at least 2 seconds between GETs and stop-on-429/5xx.
- Compare expected versus actual HTTP status and declared content type, check JSON syntax and shallow declared root type; preserve a clear distinction between match, status-only and mismatch. Full JSON Schema conformity is **not** claimed.
- No guessed endpoints, unsafe HTTP methods, tokens, credentials, arbitrary redirects, response-body storage or speculative path mutation.
- Native inspector renders per-route verification status; current Android preview remains offline.
- Additional no-network regression tests cover GitHub lookup, domain normalization, atlas binding, explicit authorization, eligible path planning, response metadata and local library roundtrip. The feature is documented in [VERIFICATION.md](docs/VERIFICATION.md).

**Status:** Automated desktop and Android build/test pipelines provide package-level validation. Domain-to-GitHub ownership and authorization cannot be inferred automatically; users must verify their permission. Real production endpoint conformance and Android physical-device interaction are not certified.

## [v0.1.0-alpha.5](https://github.com/bspippi1337/restmore/releases/tag/v0.1.0-alpha.5) · 2026-11-08

**Architectural Edition / From concept sketch to working product.**

- Rebuilt the README as an editorial product introduction, with original architectural SVG plates, native downloads and an immediately usable C11 quickstart.
- Added [ABOUT.md](ABOUT.md) with positioning, target audiences, verified limitations and copy-ready GitHub sidebar metadata. The connected GitHub integration cannot update the repository's actual About description/homepage/topics.
- Drew original vector plates: the RESTMORE mark, architect's pencil arch, mountain elevation, full three-panel console, orbital API operation map and provenance ledger.
- Built a complete [illustrated field guide](docs/field-guide.html) and companion [product page](docs/product.html), accessible HTML with local styling and no outside dependencies.
- Redesigned the GitHub Pages homepage with the architectural artwork, product narrative, native-console illustration and release links.
- Upgraded the interactive **synthetic** web demo: clickable and keyboard-accessible nodes and routes, real map/list switching, a source inspector and a copyable illustrative request template. This is explicitly not a browser-based network crawler.
- Added vector drafting marks and an original line-drawn architectural arch directly to the native C11/raylib GUI. Windows, Linux and macOS native GUI CI passed, along with ARM64 Android NativeActivity packaging.
- Distributed the complete self-contained `docs/` folder in desktop release ZIPs, including the illustrated guide and original SVG plates.
- Tightened the GitHub Pages QA workflow with JavaScript syntax validation and referenced-asset checks.
- All five Alpha 5 release jobs passed: Windows HTTPS, Linux, macOS, Android debug APK and publishing with SHA-256 checksums. GitHub Pages deployment also succeeded.

**Boundaries:** The Android APK is still an offline debug preview, not physically device-verified or store-signed. There is no installable iOS IPA. Static topology plates depict illustrative data, not live scan findings.

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
