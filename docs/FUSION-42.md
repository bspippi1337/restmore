# RESTLESS × RESTMORE / Fusion 42

**MAP THE UNSEEN. PRESERVE THE EVIDENCE.**

Branch: feature/restless-restmore-fusion-42. A reviewable experiment, NOT merged into RESTMORE main and NOT a released binary. Remote execution is off by default.

## How RESTLESS originally began

RESTLESS was created on **8 February 2026**. Its [initial repository commit](https://github.com/bspippi1337/restless/commit/203768e35e8e67ed05979f1ddcf2c6e2b57d4e6b) already described a Go-based, CLI-first API explorer: start from a domain, locate documentation/OpenAPI, extract endpoints, present them in a terminal UI and safely verify documented or narrowly seed-derived candidates. Brute force, authentication bypass and unsafe writes were explicit non-goals.

The original [RFC-0002](https://github.com/bspippi1337/restless/blob/main/legacy/docs/RFC-0002-domain-first-discovery.md), dated 8 February 2026, defined the pipeline: domain → host candidates → documents → evidence-backed endpoints → deliberately controlled verification. The historical proposal is not proof of implementation.

RESTLESS 42 later evolved into [Botanical](https://github.com/bspippi1337/restless/blob/main/docs/botanical.md): **harvest → verify → index → search → plan**. Its Go crawler stores SHA-256-addressed source bytes plus an append-only JSONL manifest, supports ETag/Last-Modified revalidation, builds an offline OpenAPI catalog and offers provenance-based search and version diffs. The [RESTLESS API Atlas guide](https://github.com/bspippi1337/restless/blob/main/docs/RESTLESS_42_API_Atlas_EN.md) also describes a GUI, parameter inspector and exports.

Legacy enginewow/discoverwow experiments went further into automatic probing of seeded URLs. Fusion 42 deliberately does not activate speculative network probing.

## Implemented on this branch

**Bidirectional archive compatibility and conditional source revalidation, entirely in C11:**

1. The authorized RESTMORE documentation crawler now emits a RESTLESS-compatible index.jsonl alongside sha256/<digest> source files, including ETag, Last-Modified, source timestamps and HTTP 200/304 audit metadata.
2. The Fusion importer reads a RESTLESS Botanical archive offline, re-verifies every source SHA-256, rejects malformed JSON, duplicate source keys, nested URL spoofing, missing archives, path traversal and mixed-host inputs, then imports **only verified OpenAPI 3 JSON** operations.
3. Revalidated records for the same URL keep the latest entry. The crawler sends conditional ETag/Last-Modified GET requests only after the existing archived bytes have been SHA-256-verified. HTTP 304 reuses the validated bytes, does not overwrite the archive and appends a new provenance record. Unexpected 304 and corrupt cache fail closed.
4. Offline relevance search ranks actual operation IDs, paths, summaries, methods and source URLs without making requests.
5. Offline contract diff reports added, removed, modified and unchanged documented operations.
6. The native raylib GUI has a new **IMPORT VERIFIED ARCHIVE** action that reuses the existing operation atlas, inspector and export workflow.
7. RESTMORE's already separate **GitHub review** and **AUTHORIZE** actions remain enforced in C before any live, bounded GET contract check.

### Try it

First build the offline C11 core:

    cmake -S . -B build-fusion -DRESTMORE_NETWORK=OFF -DRESTMORE_TESTS=ON
    cmake --build build-fusion --config Release

Import a historical RESTLESS Botanical directory:

    ./build-fusion/restmore-cli --fusion-import .restless/botanical --out fusion-atlas

Search source-backed operations without network access:

    ./build-fusion/restmore-cli --fusion-import .restless/botanical --fusion-search "list pets" --out fusion-atlas

Compare two local RESTLESS archives:

    ./build-fusion/restmore-cli --fusion-import snapshots/before --fusion-compare snapshots/after --out fusion-atlas

Verify regression suites:

    ctest --test-dir build-fusion --output-on-failure

A RESTMORE documentation crawl writes the same archive format under .restmore/<public-host>/index.jsonl, accompanied by its sha256/ content store and standard RESTMORE exports.

### Architecture

    EXPLICIT HTTPS SCOPE
            |
         AUTHORIZE
            |
       DOC HARVEST
            |
       SHA-256 BYTES ───────── RESTLESS BOTANICAL ARCHIVE
            |                           |
       index.jsonl ──────────────────────┘
            |
       OFFLINE HASH VALIDATION
            |
       DOCUMENTED OPENAPI ATLAS
            |
       +----+----------+---------------+
       |               |               |
     SEARCH           DIFF         NATIVE GUI
                                         |
                             GITHUB REVIEW + AUTHORIZE
                                         |
                             DOCUMENTED GET CONTRACTS

All archive import, search and diff operations are offline. GitHub search is a lead, not proof of domain ownership. No endpoint verification is permitted merely because a source was imported.

## Technical tradeoffs and honesty

- **No guessing:** A route exists in the atlas only when a verified source actually documents it.
- **No arbitrary authority:** Approval to inspect the GitHub source is distinct from documented authorization to test the API host.
- **No silent truncation:** The current static RESTMORE catalog has finite document and operation capacities; exceeding them fails explicitly. Scaling larger datasets will require dynamic indexing or sharding, rather than pretending memory is unlimited.
- **No hidden execution:** No auth bypass, credential replay, speculative endpoint calls or mutating HTTP methods. Robots, TLS certificate checks, public-address restrictions, pacing and explicit scope remain mandatory.
- **SHA-256 is content integrity**, not publisher authentication or proof that an API operation is deployed.
- **Already integrated:** RESTLESS-style append-only evidence manifests, SHA validation, ETag/Last-Modified conditional GET and verified HTTP 304 reuse, offline indexing, offline route search and response-contract diff.
- **Not yet ported:** RESTLESS Go's full RFC 9309 robots precedence, YAML and local spec reference resolution, and text-reference search. These remain independent modules until there is real tested C11 parity. The 304 feature has compile and offline cache-integrity tests, but its remote conditional-response behavior has not yet been tested against a controlled HTTPS integration server.

## Test evidence

The dedicated [Fusion 42 CI](../.github/workflows/fusion-42.yml) runs C11 SHA/source/authorization tests across Linux, Windows and macOS and verifies that a network-enabled Linux core compiles. Tests cover corrupted archives, source host binding, ETag/Last-Modified metadata and cache recovery, latest-manifest-entry behavior, offline query, contract diff and adversarial JSONL.

This branch is intentionally separate from main, and suitable for technical code review. No production security certification or real-target authorization is claimed.

[RESTLESS source](https://github.com/bspippi1337/restless) · [RESTLESS Botanical](https://github.com/bspippi1337/restless/tree/main/internal/botanical) · [RESTMORE](https://github.com/bspippi1337/restmore)
