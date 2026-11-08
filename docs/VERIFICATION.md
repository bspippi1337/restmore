# RESTMORE / Verification Mode

> **MAP THE UNSEEN. VERIFY WHAT IS DOCUMENTED.**

A bounded, evidence-first contract check, not a fuzzer or a live-system inventory. The feature is implemented in **ISO C11** and presented in the native raylib GUI.

## The first-click promise

1. **Enter only a public domain.** For example, `api.your-company.tld`. Custom ports, URL paths, credentials, fragments, IP literals and reserved/local hostnames are refused.
2. Click **MAP / REVIEW GITHUB**. RESTMORE opens a GitHub repository **search for that domain** in your browser, before making *any* verification HTTP requests. A domain does not inherently have a unique GitHub repository. Search results are candidates, **not proof of ownership**. The human must review any relevant repository and independently confirm permission to inspect the target.
3. Back in the native GUI, examine the selected catalog scope and source-binding warning. Click **I REVIEWED GITHUB & HAVE PERMISSION** as a separate, deliberate acknowledgment of your review and authorization to test that server. GitHub search alone is not an identity check.
4. Click **AUTHORIZE / N DOCUMENTED GET CHECKS**. The C11 engine requires *both* manual review and this independent authorization, plus a source-bound atlas, before allowing requests. Changing the domain or reloading the atlas invalidates pending approval. The web demo does not perform this action.
5. The native inspector displays each checked operation's path, expected HTTP status, observed status and verdict. An absent documented response expectation makes an endpoint ineligible for automatic checking.

**No API verification request is sent on the first MAP click.**

## Exactly what is checked

The verifier selects **5, 25 or the full eligible atlas**, as chosen by the operator before the GitHub review and authorization step, from existing `GET` operations from the already imported RESTMORE atlas/library. Paths must be literal (no `{parameter}`, wildcards, escapes, query strings, or guessed segments). It refuses obvious authentication, administrative, money movement and lifecycle paths even if declared `GET`, since poorly designed services can have side effects on read requests.

A candidate must declare a concrete **200 or 204** response in OpenAPI. For each eligible request, the verifier checks:

- Expected response status **versus** observed HTTP status.
- When declared, response **Content-Type**.
- For JSON responses, valid JSON syntax.
- When the OpenAPI JSON schema declares a simple root `type`, the returned root's type: object, array, string, number, integer, boolean or null.

Results distinguish **MATCH** from **STATUS ONLY**, **MISMATCH**, **SKIPPED** and **ERROR**. Click **EXPORT VERIFICATION JSON** to create `restmore-verification.json` with the normalized domain, recorded consent, expected and observed HTTP codes/media types, JSON root-type expectation and source SHA-256. It does not store response bodies or secrets. A status-only result is not treated as comprehensive proof that the output is correct.

**Not implemented:** full JSON Schema constraint validation, authentication-specific responses, parameter substitution, schema `$ref` resolution, response-body retention, or API request replay. A matching root type alone does not prove every field/value is correct.

## Network boundaries

- **Two separate human gestures** required: GitHub review acknowledgment and explicit authorization for the exact normalized host. Both are enforced in the C11 network worker, independently of discovery consent.
- HTTPS only, certificate verification, public-network socket checks and **no redirects**.
- Robots policy checked first; a failure outside 200/404/410 stops the run.
- Conservative **at least 2 seconds** between verification requests, greater robots delay wins.
- Maximum **5** literal documented GET requests, each capped at **128 KiB** and a bounded timeout.
- No cookies, access tokens, API keys, authorization headers, POST, PUT, PATCH, DELETE, fuzzing, guessed paths or endpoint mutation.
- Remote HTTP 429 or 5xx stops the run without retry. Response bodies are analyzed transiently and discarded, **not stored**.
- On an offline Android build (`RESTMORE_NETWORK=OFF`) verification network execution is disabled. The GitHub review step and local catalog visualization can still be used.

The native Authorize control stays **disabled until the atlas identifies the selected domain**. For a local OpenAPI file, a public root-origin `servers[].url` can supply that binding; a random sample spec cannot. A matching host still does not establish permission: **GitHub search does not prove domain ownership or legal authorization**. Follow your organization's rules and only check targets you have permission to inspect.

## Response contracts in the library

RESTMORE now preserves these operation fields on OpenAPI import and `library.json` roundtrips:

```json
{
  "method": "GET",
  "path": "/v1/pets",
  "expected_status": 200,
  "expected_content_type": "application/json",
  "expected_json_type": "array"
}
```

This is an illustrative fragment. The normal library adds method details, source URL and document SHA-256 provenance. Existing libraries with no declared response contract are still readable; their operations will simply not be automatically probed.

## Engineering and tests

Pure C11 planning: `src/verification.c`. OpenAPI response extraction: `src/json.c`. Network worker: `src/network.c` or disabled-network stub `src/network_off.c`. GUI gate/results: `src/gui.c`.

`tests/test_verification.c` exercises public-domain normalization, GitHub search formatting, both positive and negative GitHub-review/authorization gates, JSON report validity, exact GET selection, forbidden paths, response metadata, simple JSON root type, and library roundtrip. The tests require **no network access**.

[Contract verification field notes](CONTRACT-VERIFICATION.md) · [Product site](index.html) · [Architecture](ARCHITECTURE.md) · [Build guide](BUILD.md) · [Source](https://github.com/bspippi1337/restmore)
