# OPS 42 Analyst Workbench · Alpha 9 candidate

**Status:** experimental portfolio milestone on a feature branch, not an official
Norwegian intelligence service product. No affiliation with Etterretningstjenesten
or Forsvaret. Only public job-description themes inform the engineering priorities.

## Intended operator workflow

1. **Open sample.** Start on the Android Atlas, prepopulated from the bundled
   OpenAPI example. The native workbench has a separate Desk, Atlas and Evidence
   view. This mobile APK deliberately has **no network execution**.
2. **Inspect.** Select a documented route to see HTTP method, path, declared
   summary, expected response contract, source identity and source SHA-256.
   The app says *recorded, not reverified*. It never invents observations.
3. **Export evidence on desktop.** Use the same pure C11 engine with
   `--import examples/openapi-petstore.json --ops-brief --out evidence`.
   `ops-brief.json` now uses format `restmore-ops-42/v2`, with a
   source-resolvable `sources` collection and per-route `source_url`,
   operation identifier, summary, documented MIME and JSON root type.
4. **Reproduce.** A source-metadata fingerprint is deterministic for identical
   catalogs. All free-text fields use explicit length-prefix boundaries, so
   delimiters embedded in values cannot silently change field interpretation.
   The digest covers source URLs, content types, response metadata and
   declared route contracts.
5. **Separate authority from evidence.** Imported documents never authorize
   execution. Even the desktop's separately consent-gated verification only
   tests documented safe GET contracts. No credential replay, guessing,
   non-GET mutation or vulnerability exploitation is added by this milestone.

## Why this demonstrates relevant engineering skills

The public November 2026 vacancy "Erfaren pentester søkes til cyberoperasjoner"
values technical curiosity, creative problem solving, information security,
reliable tool/procedure improvement, operating-system and embedded experience.
OPS 42 is a transparent demonstration of these *general, public* skill areas:
portable ISO C11, Android NativeActivity, native raylib, structured source
provenance, reproducible builds, automated regression suites and explicit
trust boundaries.

Vacancy reference: https://www.forsvaret.no/jobb/ledige-stillinger/cyberoperator
This is not an endorsement, accreditation, operational platform or permission
to test a third-party system. The application does not claim suitability for
classified or regulated environments.

## Acceptance gates

| Area | Gate | Current evidence |
| --- | --- | --- |
| C11 source integrity | Repeated offline export is byte-reproducible | C11 test suite |
| Source provenance | Report includes route source URL, original digest and source ledger | C11 test suite |
| Collision resistance | Distinct field boundaries yield distinct fingerprints | C11 negative test |
| Human authorization | Import never means permission or makes a network request | Offline contract / tests |
| Android usability | Native portrait viewport, working buttons, readable source inspector | Android CI build only; physical QA pending |
| Distribution | Debug-signed APK with independently checkable SHA-256 | Android packaging workflow |
| Platform scope | Android preview offline; desktop C11 supports separate, bounded HTTPS checks | Build definitions |
| Privacy | No credentials or response bodies in OPS 42 evidence reports | Source and regression review |

## Explicit limitations

- Fingerprints are **SHA-256 checksums of serialized evidence**, not digital
  signatures, publisher authentication, chain-of-custody certification or proof
  that a remote service currently behaves as documented.
- Manifest v2 is a format change. A previous v1 fingerprint **must not** be
  compared directly as if it used the same serialization.
- The Android preview can open the bundled local example, but an Android system
  document picker and reliable public file export are not yet implemented.
- Phone screenshots and runtime touch/rotation/storage tests are still required.
  CI compilation does not assert physical-device correctness.
- No live pentest claims, vulnerabilities, secret sources, access entitlements,
  intelligence classifications or official E-tjenesten affiliation.

## Reproducible commands

```sh
cmake -S . -B build-analyst -DRESTMORE_NETWORK=OFF -DRESTMORE_GUI=OFF -DRESTMORE_TESTS=ON
cmake --build build-analyst --parallel 2
ctest --test-dir build-analyst --output-on-failure
./build-analyst/restmore-cli --import examples/openapi-petstore.json --ops-brief --out evidence
```

The output directory contains `library.json`, `ops-brief.json` and other
offline atlas exports. `ops-brief.json` always states
`mission_state=REVIEW_REQUIRED`, `target_execution=false` and
`permission_conferred=false`.

**Follow-on milestone:** an Android Storage Access Framework bridge for native
import/export; independent manifest verification command; physical device
screenshot regression before calling the mobile UI ready for general use.
