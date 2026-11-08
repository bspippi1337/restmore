#!/usr/bin/env python3
"""RESTMORE Fusion 42 / controlled HTTPS end-to-end integration.
RUN ONLY in the isolated Linux CI fixture build, never against a public target.
This test intentionally requires Linux root for local port 443 + /etc/hosts.
The C11 executable is real; TLS verification, curl, robots, archive and revalidation run unchanged.
"""
from __future__ import annotations

import hashlib
import http.server
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time

HOST = "restmore-e2e-fixture.example.org"
BASE_URL = f"https://{HOST}/docs/openapi.json"
ETAG_1 = '"fusion-revision-1"'
ETAG_2 = '"fusion-revision-2"'
LAST_MODIFIED = "Wed, 21 Oct 2015 07:28:00 GMT"


def need(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def spec(revision: int) -> bytes:
    return json.dumps({
        "openapi": "3.1.0",
        "info": {"title": "RESTMORE private CI contract fixture", "version": str(revision)},
        "servers": [{"url": f"https://{HOST}"}],
        "paths": {
            "/v1/pets": {
                "get": {
                    "operationId": "listPets",
                    "summary": "List documented fixture pets",
                    "responses": {"200": {"content": {
                        "application/json": {"schema": {"type": "array"}}
                    }}}
                }
            }
        }
    }, separators=(",", ":")).encode()


class Fixture(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    docs_requests = 0
    robots_requests = 0
    unexpected_requests = 0
    conditional_requests = 0
    state = {"revision": 1, "deny": False, "rate_limit": False, "server_error": False}

    def log_message(self, *_args):
        pass

    def send(self, status: int, payload: bytes = b"", *, content_type: str = "text/plain",
             headers: dict[str, str] | None = None) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.end_headers()
        if payload:
            self.wfile.write(payload)

    def do_GET(self):
        if self.path == "/robots.txt":
            Fixture.robots_requests += 1
            policy = "User-agent: *\nDisallow: /docs\n" if Fixture.state["deny"] else \
                     "User-agent: *\nAllow: /\n"
            self.send(200, policy.encode())
            return
        if self.path != "/docs/openapi.json":
            Fixture.unexpected_requests += 1
            self.send(404)
            return
        Fixture.docs_requests += 1
        if Fixture.state["rate_limit"]:
            self.send(429, b"slow down")
            return
        if Fixture.state["server_error"]:
            self.send(503, b"server unavailable")
            return
        rev = int(Fixture.state["revision"])
        tag = ETAG_1 if rev == 1 else ETAG_2
        if self.headers.get("If-None-Match") or self.headers.get("If-Modified-Since"):
            Fixture.conditional_requests += 1
        if self.headers.get("If-None-Match") == tag:
            self.send(304, headers={"ETag": tag, "Last-Modified": LAST_MODIFIED})
            return
        self.send(200, spec(rev), content_type="application/json",
                  headers={"ETag": tag, "Last-Modified": LAST_MODIFIED})


def run(cli: str, output: Path, *, authorized: bool = True,
        execute: bool = True, trust: bool = True) -> subprocess.CompletedProcess:
    env = os.environ.copy()
    env["RESTMORE_CI_LOOPBACK_FIXTURE"] = HOST
    if trust:
        env["CURL_CA_BUNDLE"] = str(run.certfile)
        env["RESTMORE_CI_CA_FILE"] = str(run.certfile)
        env["SSL_CERT_FILE"] = str(run.certfile)
    else:
        env.pop("CURL_CA_BUNDLE", None)
        env.pop("RESTMORE_CI_CA_FILE", None)
        env.pop("SSL_CERT_FILE", None)
    args = [cli, "--out", str(output), "--max-pages", "1", "--max-depth", "0",
            "--delay-ms", "2000", BASE_URL]
    if execute:
        args.append("--execute")
    if authorized:
        args.append("--authorized")
    return subprocess.run(args, text=True, capture_output=True,
                          timeout=35, env=env, check=False)


def records(directory: Path) -> list[dict]:
    index = directory / "index.jsonl"
    return [json.loads(line) for line in index.read_text().splitlines() if line.strip()]


def check_run(cli: str, temp: Path) -> None:
    output = temp / "harvest"
    directory = output / HOST

    # Neither a domain preview nor --execute without --authorized may contact the API.
    for execute, authorize in [(False, False), (False, True), (True, False)]:
        before = (Fixture.robots_requests, Fixture.docs_requests)
        result = run(cli, output, execute=execute, authorized=authorize)
        need(result.returncode == (1 if execute and not authorize else 0),
             f"preview/denied authorization: {result.returncode}: {result.stderr}")
        need(before == (Fixture.robots_requests, Fixture.docs_requests),
             "HTTP traffic occurred before an explicit authorization")
    print("PASS 01 / Preview and missing authorization cause zero HTTP requests", flush=True)

    # Never disable peer verification even for a local TLS fixture.
    before = (Fixture.robots_requests, Fixture.docs_requests)
    result = run(cli, output, trust=False)
    need(result.returncode != 0, "Self-signed fixture TLS was accepted without trust root")
    need(before == (Fixture.robots_requests, Fixture.docs_requests),
         "Untrusted TLS certificate sent an application-level request")
    print("PASS 02 / Untrusted TLS fails closed", flush=True)

    result = run(cli, output)
    need(result.returncode == 0, f"first authorized GET failed: {result.stderr}")
    data = records(directory)
    need(len(data) == 1, "Expected single first archive record")
    first = data[0]
    expected_hash = hashlib.sha256(spec(1)).hexdigest()
    need(first["response_status"] == 200 and first["sha256"] == expected_hash,
         f"Incorrect first source digest or response: {first}")
    need(first["etag"] == ETAG_1, "ETag was not recorded")
    stored = directory / "sha256" / expected_hash
    need(stored.read_bytes() == spec(1), "Stored bytes do not match verified source")
    need((directory / "library.json").exists(), "Atlas not generated from downloaded OpenAPI")
    print("PASS 03 / Real authorized TLS GET -> OpenAPI atlas -> content-addressed source", flush=True)

    before_requests = Fixture.docs_requests
    result = run(cli, output)
    need(result.returncode == 0, f"304 revalidation failed: {result.stderr}")
    data = records(directory)
    need(len(data) == 2 and data[-1]["response_status"] == 304,
         f"Expected new 304 record, got {data[-1] if data else None}")
    need(data[-1]["sha256"] == expected_hash, "304 changed source digest")
    need(stored.read_bytes() == spec(1), "304 overwrote cached source bytes")
    need(Fixture.docs_requests == before_requests + 1, "304 did not involve exactly one GET")
    need(Fixture.conditional_requests >= 1, "Conditional request headers were not sent")
    print("PASS 04 / If-None-Match -> HTTP 304 -> verified source reuse", flush=True)

    # Content-addressed bytes must be verified locally BEFORE any document GET.
    stored.write_bytes(b"tampered-digest-should-fail-closed")
    before_requests = Fixture.docs_requests
    result = run(cli, output)
    need(result.returncode != 0, "Corrupt cache was silently accepted")
    need("SHA-256" in result.stderr, f"No corruption explanation: {result.stderr}")
    need(Fixture.docs_requests == before_requests, "Document was fetched after cache corruption")
    stored.write_bytes(spec(1))
    print("PASS 05 / Tampered archive blocks the next document request", flush=True)

    # A source revision must create a NEW digest and append a new record, no overwrites.
    Fixture.state["revision"] = 2
    result = run(cli, output)
    need(result.returncode == 0, f"ETag refresh 200 failed: {result.stderr}")
    new_hash = hashlib.sha256(spec(2)).hexdigest()
    data = records(directory)
    need(data[-1]["response_status"] == 200 and data[-1]["sha256"] == new_hash,
         "ETag changed but new source was not archived")
    need((directory / "sha256" / new_hash).read_bytes() == spec(2), "New source has wrong bytes")
    need(stored.read_bytes() == spec(1), "Revalidation overwrote the historical source")
    result = run(cli, output)
    need(result.returncode == 0 and records(directory)[-1]["response_status"] == 304,
         f"Second ETag revision revalidation: {result.stderr}")
    print("PASS 06 / New ETag -> new SHA archive -> next 304", flush=True)

    # robots disallow, 429 and 5xx must stop, never guess alternate paths or retry.
    Fixture.state["deny"] = True
    before_requests = Fixture.docs_requests
    result = run(cli, output)
    need(result.returncode != 0, "robots disallow unexpectedly allowed crawl")
    need(Fixture.docs_requests == before_requests, "robots disallow was ignored")
    Fixture.state["deny"] = False
    print("PASS 07 / robots disallow blocks documentation GET", flush=True)

    for key in ["rate_limit", "server_error"]:
        Fixture.state[key] = True
        before_requests = Fixture.docs_requests
        before_rows = len(records(directory))
        result = run(cli, output)
        Fixture.state[key] = False
        need(result.returncode != 0, f"{key}: did not fail closed")
        need(Fixture.docs_requests == before_requests + 1, f"{key}: automatic retry occurred")
        need(len(records(directory)) == before_rows, f"{key}: error polluted archive")
    need(Fixture.unexpected_requests == 0, "Crawler sent unexpected paths")
    print("PASS 08 / 429 and 503 stop without retry or archive pollution", flush=True)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: sudo python3 tests/e2e_https_fixture.py /absolute/path/to/restmore-cli")
    if os.name != "posix" or os.geteuid() != 0:
        raise SystemExit("This self-contained integration test requires sudo on ephemeral Linux CI")
    cli = str(Path(sys.argv[1]).resolve())
    need(Path(cli).exists(), f"Missing CI-built C11 binary: {cli}")
    with tempfile.TemporaryDirectory(prefix="restmore-e2e-") as folder:
        temp = Path(folder)
        cert, key = temp / "cert.pem", temp / "key.pem"
        subprocess.run([
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
            "-keyout", str(key), "-out", str(cert),
            "-subj", f"/CN={HOST}", "-addext", f"subjectAltName=DNS:{HOST}"
        ], capture_output=True, text=True, check=True)
        run.certfile = cert
        hosts = Path("/etc/hosts")
        with hosts.open("a", encoding="ascii") as file:
            file.write(f"\n127.0.0.1 {HOST}\n")
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 443), Fixture)
        server.daemon_threads = True
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(certfile=cert, keyfile=key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            check_run(cli, temp)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=4)
    print("Fusion 42 REAL TLS 200/304/TAMPER E2E: ALL SCENARIOS PASS", flush=True)


if __name__ == "__main__":
    main()
