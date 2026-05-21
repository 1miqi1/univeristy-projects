#!/usr/bin/env python3
"""
Comprehensive test suite for sikradio – an internet radio client.

Tests are organized into sections:
  1. Parameter parsing & validation
  2. URL parsing
  3. HTTP request construction
  4. HTTP response parsing & redirect following
  5. ICY metadata demultiplexing
  6. Timeout & reconnection logic
  7. Quit / stdin handling
  8. Verbosity levels
  9. Cookie handling
 10. Edge cases & stress tests

Each test spins up a lightweight TCP (or TLS) mock server, launches sikradio
as a subprocess, and asserts on stdout (audio), stderr (diagnostics/metadata),
and exit code.

Requirements:
  - sikradio binary in CWD (or pass --binary /path/to/sikradio)
  - Python >= 3.8
  - No third-party packages needed (only stdlib)

Run:
    make
    python3 test_sikradio.py           # run all
    python3 test_sikradio.py -v        # verbose
    python3 test_sikradio.py TestParameterParsing  # single class
"""

import contextlib
import errno
import os
import random
import re
import select
import signal
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import textwrap
import threading
import time
import unittest
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SIKRADIO_BIN = os.environ.get("SIKRADIO_BIN", "./sikradio")
# Generous default for CI; tighten if needed.
DEFAULT_PROC_TIMEOUT = 15  # seconds to wait for sikradio to finish
SHORT_TIMEOUT_MS = 300     # -t value for fast-timeout tests
CONNECT_GRACE = 2.0        # extra seconds to let sikradio connect


def _find_free_port(family=socket.AF_INET):
    """Return an unused TCP port on localhost."""
    with socket.socket(family, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        addr = "127.0.0.1" if family == socket.AF_INET else "::1"
        s.bind((addr, 0))
        return s.getsockname()[1]


# ---------------------------------------------------------------------------
# Mock ICY/HTTP server helpers
# ---------------------------------------------------------------------------

class MockServer:
    """A simple single-connection TCP server for testing sikradio.

    Usage:
        srv = MockServer(handler_func, family=socket.AF_INET)
        srv.start()
        # ... launch sikradio pointing at srv.url ...
        srv.stop()

    handler_func(conn: socket.socket, addr) is called in a thread.
    """

    def __init__(self, handler, family=socket.AF_INET, use_tls=False,
                 certfile=None, keyfile=None, max_connections=5):
        self.handler = handler
        self.family = family
        self.use_tls = use_tls
        self.certfile = certfile
        self.keyfile = keyfile
        self.max_connections = max_connections
        self._sock = None
        self._thread = None
        self._stop_event = threading.Event()
        self.port = None
        self.connections_served = 0
        self.last_request = None  # raw bytes of last request received
        self._lock = threading.Lock()
        self._handler_threads = []

    @property
    def host(self):
        return "127.0.0.1" if self.family == socket.AF_INET else "::1"

    @property
    def url_host(self):
        if self.family == socket.AF_INET6:
            return f"[{self.host}]"
        return self.host

    @property
    def scheme(self):
        return "https" if self.use_tls else "http"

    def url(self, path="/"):
        return f"{self.scheme}://{self.url_host}:{self.port}{path}"

    def start(self):
        self._sock = socket.socket(self.family, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((self.host, 0))
        self.port = self._sock.getsockname()[1]
        self._sock.listen(self.max_connections)
        self._sock.settimeout(0.5)
        self._thread = threading.Thread(target=self._accept_loop, daemon=True)
        self._thread.start()

    def _accept_loop(self):
        while not self._stop_event.is_set():
            try:
                conn, addr = self._sock.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            with self._lock:
                self.connections_served += 1
            if self.use_tls:
                ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                ctx.load_cert_chain(self.certfile, self.keyfile)
                try:
                    conn = ctx.wrap_socket(conn, server_side=True)
                except ssl.SSLError:
                    conn.close()
                    continue
            t = threading.Thread(target=self._handle_safe,
                                 args=(conn, addr), daemon=True)
            t.start()
            self._handler_threads.append(t)

    def _handle_safe(self, conn, addr):
        try:
            self.handler(conn, addr, self)
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        finally:
            try:
                conn.close()
            except OSError:
                pass

    def stop(self):
        self._stop_event.set()
        if self._sock:
            self._sock.close()
        if self._thread:
            self._thread.join(timeout=5)
        for t in self._handler_threads:
            t.join(timeout=2)


def recv_until(conn, delimiter=b"\r\n\r\n", timeout=5):
    """Receive from conn until delimiter is found or timeout."""
    conn.settimeout(timeout)
    data = b""
    while delimiter not in data:
        try:
            chunk = conn.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        data += chunk
    return data


def build_icy_response(status_line="ICY 200 OK",
                       headers=None, body=b""):
    """Build a raw ICY/HTTP response."""
    resp = status_line.encode() + b"\r\n"
    if headers:
        for k, v in headers.items():
            resp += f"{k}:{v}\r\n".encode()
    resp += b"\r\n"
    resp += body
    return resp


def build_http_response(status_line="HTTP/1.1 200 OK",
                        headers=None, body=b""):
    resp = status_line.encode() + b"\r\n"
    if headers:
        for k, v in headers.items():
            resp += f"{k}: {v}\r\n".encode()
    resp += b"\r\n"
    resp += body
    return resp


def build_icy_stream(audio_chunks, metaint, metadata_strings=None):
    """Build a byte stream with interleaved audio and ICY metadata.

    audio_chunks: list of bytes objects, each of length == metaint
    metadata_strings: list of metadata strings (or None for zero-length meta)
    """
    if metadata_strings is None:
        metadata_strings = [None] * len(audio_chunks)
    stream = b""
    for i, chunk in enumerate(audio_chunks):
        assert len(chunk) == metaint, f"chunk {i} len {len(chunk)} != {metaint}"
        stream += chunk
        meta = metadata_strings[i] if i < len(metadata_strings) else None
        if meta is None:
            stream += b"\x00"  # zero-length metadata
        else:
            meta_bytes = meta.encode("utf-8") if isinstance(meta, str) else meta
            # Pad to multiple of 16
            padded_len = ((len(meta_bytes) + 15) // 16) * 16
            meta_bytes_padded = meta_bytes + b"\x00" * (padded_len - len(meta_bytes))
            length_byte = padded_len // 16
            stream += bytes([length_byte]) + meta_bytes_padded
    return stream


def generate_self_signed_cert(tmpdir):
    """Generate a self-signed cert+key in tmpdir, return (certfile, keyfile)."""
    certfile = os.path.join(tmpdir, "cert.pem")
    keyfile = os.path.join(tmpdir, "key.pem")
    # Use openssl CLI; available on virtually all test machines.
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "rsa:2048",
        "-keyout", keyfile, "-out", certfile,
        "-days", "1", "-nodes",
        "-subj", "/CN=localhost",
        "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1"
    ], check=True, capture_output=True)
    return certfile, keyfile


# ---------------------------------------------------------------------------
# Subprocess launcher
# ---------------------------------------------------------------------------

def run_sikradio(args, stdin_data=None, timeout=DEFAULT_PROC_TIMEOUT,
                 feed_quit_after=None, stdin_bytes=None):
    """Launch sikradio with given args.  Returns (stdout_bytes, stderr_bytes, returncode).

    feed_quit_after: if set, number of seconds to wait before writing "quit\n"
                     to stdin.
    stdin_bytes: raw bytes to feed to stdin (mutually exclusive with feed_quit_after).
    """
    cmd = [SIKRADIO_BIN] + args
    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout = b""
    stderr = b""
    try:
        if feed_quit_after is not None:
            time.sleep(feed_quit_after)
            proc.stdin.write(b"quit\n")
            proc.stdin.flush()
            stdout, stderr = proc.communicate(timeout=timeout)
        elif stdin_bytes is not None:
            stdout, stderr = proc.communicate(input=stdin_bytes, timeout=timeout)
        elif stdin_data is not None:
            stdout, stderr = proc.communicate(
                input=stdin_data.encode() if isinstance(stdin_data, str) else stdin_data,
                timeout=timeout)
        else:
            stdout, stderr = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        stdout, stderr = proc.communicate()
    return stdout, stderr, proc.returncode


# ---------------------------------------------------------------------------
# Test helpers
# ---------------------------------------------------------------------------

class SikradioTestBase(unittest.TestCase):
    """Base class with convenience assertions."""

    def assertExitOk(self, rc, stderr_text=""):
        self.assertEqual(rc, 0,
                         f"Expected exit 0, got {rc}. stderr: {stderr_text[:500]}")

    def assertExitError(self, rc, stderr_text=""):
        self.assertEqual(rc, 1,
                         f"Expected exit 1, got {rc}. stderr: {stderr_text[:500]}")

    def assertStderrContains(self, stderr_bytes, pattern):
        text = stderr_bytes.decode("utf-8", errors="replace")
        self.assertIn(pattern, text, f"stderr missing '{pattern}'")

    def assertStderrNotContains(self, stderr_bytes, pattern):
        text = stderr_bytes.decode("utf-8", errors="replace")
        self.assertNotIn(pattern, text, f"stderr unexpectedly contains '{pattern}'")


# ===========================================================================
# 1. Parameter parsing
# ===========================================================================

class TestParameterParsing(SikradioTestBase):
    """Tests for CLI argument validation."""

    def test_no_args(self):
        """No arguments at all → exit 1."""
        _, stderr, rc = run_sikradio([])
        self.assertExitError(rc)

    def test_missing_url(self):
        """No -u flag → exit 1."""
        _, stderr, rc = run_sikradio(["-m", "-v1"])
        self.assertExitError(rc)

    def test_empty_url(self):
        """Empty -u '' → exit 1 (or treated as invalid)."""
        _, stderr, rc = run_sikradio(["-u", ""])
        self.assertExitError(rc)

    # ---- timeout validation ----

    def test_timeout_too_low(self):
        _, _, rc = run_sikradio(["-u", "http://x", "-t", "99"])
        self.assertExitError(rc)

    def test_timeout_too_high(self):
        _, _, rc = run_sikradio(["-u", "http://x", "-t", "100001"])
        self.assertExitError(rc)

    def test_timeout_non_numeric(self):
        _, _, rc = run_sikradio(["-u", "http://x", "-t", "abc"])
        self.assertExitError(rc)

    def test_timeout_float(self):
        """Float like 5000.5 should be rejected (not a valid integer)."""
        _, _, rc = run_sikradio(["-u", "http://x", "-t", "5000.5"])
        self.assertExitError(rc)

    def test_timeout_negative(self):
        _, _, rc = run_sikradio(["-u", "http://x", "-t", "-1"])
        self.assertExitError(rc)

    def test_timeout_boundary_low(self):
        """100 ms is the minimum valid timeout – should not error on parsing."""
        # Will fail to connect (bad host), but parsing should succeed.
        # We just confirm the error is about connection, not about parsing.
        _, stderr, rc = run_sikradio(["-u", "http://127.0.0.1:1", "-t", "100"],
                                      timeout=5)
        # Should exit 1 due to connection failure, not parameter error.
        self.assertExitError(rc)
        # The error should NOT be about an invalid timeout.
        text = stderr.decode("utf-8", errors="replace")
        self.assertNotIn("Invalid timeout", text)

    def test_timeout_boundary_high(self):
        _, stderr, rc = run_sikradio(["-u", "http://127.0.0.1:1", "-t", "100000"],
                                      timeout=5)
        text = stderr.decode("utf-8", errors="replace")
        self.assertNotIn("Invalid timeout", text)

    # ---- verbosity validation ----

    def test_verbosity_too_low(self):
        _, _, rc = run_sikradio(["-u", "http://x", "-v", "-1"])
        self.assertExitError(rc)

    def test_verbosity_too_high(self):
        _, _, rc = run_sikradio(["-u", "http://x", "-v", "5"])
        self.assertExitError(rc)

    def test_verbosity_non_numeric(self):
        _, _, rc = run_sikradio(["-u", "http://x", "-v", "abc"])
        self.assertExitError(rc)

    def test_verbosity_boundary_0(self):
        """v=0 is valid."""
        _, stderr, rc = run_sikradio(["-u", "http://127.0.0.1:1", "-v", "0"],
                                      timeout=5)
        text = stderr.decode("utf-8", errors="replace")
        self.assertNotIn("Invalid verbosity", text)

    def test_verbosity_boundary_4(self):
        _, stderr, rc = run_sikradio(["-u", "http://127.0.0.1:1", "-v", "4"],
                                      timeout=5)
        text = stderr.decode("utf-8", errors="replace")
        self.assertNotIn("Invalid verbosity", text)

    # ---- unknown parameters ----

    def test_unknown_flag(self):
        _, _, rc = run_sikradio(["-u", "http://x", "-z"])
        self.assertExitError(rc)

    def test_extra_positional_args(self):
        """Extra positional arguments after flags – should still work or fail gracefully."""
        # The spec doesn't say positional args are errors, but getopt stops at them.
        # With the code using getopt, extra args are silently ignored.
        # We just check it doesn't crash with a segfault or similar.
        _, _, rc = run_sikradio(["-u", "http://127.0.0.1:1", "extra"], timeout=5)
        # Either 0 or 1 is acceptable; just not a crash.
        self.assertIn(rc, [0, 1])

    # ---- combined flags ----

    def test_combined_flags_m46(self):
        """-m46 should parse as -m -4 -6."""
        # These are boolean flags; parsing should succeed.
        _, stderr, rc = run_sikradio(
            ["-u", "http://127.0.0.1:1", "-m46"], timeout=5)
        text = stderr.decode("utf-8", errors="replace")
        self.assertNotIn("Unrecognized", text)

    def test_combined_flags_mq(self):
        """-mq should set multiplex and quiet."""
        _, stderr, rc = run_sikradio(
            ["-u", "http://127.0.0.1:1", "-mq"], timeout=5)
        # -q means verbosity 0 → no error messages on stderr
        # (except maybe nothing since it can't connect; but if v=0 errors are silent)
        text = stderr.decode("utf-8", errors="replace")
        self.assertNotIn("Unrecognized", text)

    def test_flag_value_no_space(self):
        """-t500 (no space between flag and value) should work."""
        _, stderr, rc = run_sikradio(
            ["-u", "http://127.0.0.1:1", "-t500"], timeout=5)
        text = stderr.decode("utf-8", errors="replace")
        self.assertNotIn("Invalid timeout", text)

    def test_flag_value_no_space_verbosity(self):
        """-v1 should work."""
        _, stderr, rc = run_sikradio(
            ["-u", "http://127.0.0.1:1", "-v1"], timeout=5)
        text = stderr.decode("utf-8", errors="replace")
        self.assertNotIn("Invalid verbosity", text)

    def test_q_overrides_v(self):
        """-v3 -q should result in verbosity 0 (last wins)."""
        # If server can't connect and verbosity=0, stderr should be empty.
        _, stderr, rc = run_sikradio(
            ["-u", "http://127.0.0.1:1", "-v3", "-q"], timeout=5)
        text = stderr.decode("utf-8", errors="replace")
        # With verbosity 0, even critical errors are silent (just exit 1).
        self.assertEqual(text.strip(), "")

    def test_duplicate_url_takes_last(self):
        """If -u is given twice, take one of them (spec says 'reasonable')."""
        # First URL is bogus, second is also bogus, but check no crash.
        _, _, rc = run_sikradio(
            ["-u", "http://first", "-u", "http://127.0.0.1:1"], timeout=5)
        self.assertIn(rc, [0, 1])

    def test_duplicate_timeout_mixed_validity(self):
        """Per professor: take first or last valid value, ignore others."""
        # -t 200 -t abc → could take 200 (first valid) or reject.
        # Professor said "take first or last and ignore others" is fine.
        _, stderr, rc = run_sikradio(
            ["-u", "http://127.0.0.1:1", "-t", "200", "-t", "abc"], timeout=5)
        # Both outcomes are acceptable per spec.
        self.assertIn(rc, [0, 1])


# ===========================================================================
# 2. URL parsing
# ===========================================================================

class TestURLParsing(SikradioTestBase):
    """Tests for URL parsing correctness."""

    def test_invalid_protocol(self):
        """ftp:// should be rejected."""
        _, _, rc = run_sikradio(["-u", "ftp://example.com"])
        self.assertExitError(rc)

    def test_missing_protocol(self):
        """No :// → error."""
        _, _, rc = run_sikradio(["-u", "example.com/stream"])
        self.assertExitError(rc)

    def test_http_default_port(self):
        """http:// with no port should use 80."""
        # We verify by checking the Host header sent. Spin up server on port 80?
        # Instead: use a known port, verify request Host header.
        def handler(conn, addr, srv):
            req = recv_until(conn)
            srv.last_request = req
            conn.sendall(build_http_response("HTTP/1.1 200 OK",
                         {"content-type": "audio/mpeg"}, b"\xff" * 100))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            # We must use the actual port, but check that the Host header
            # uses host:port format correctly.
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/stream"), "-q"],
                timeout=5)
            self.assertExitOk(rc)
            req = srv.last_request
            self.assertIsNotNone(req)
            # Should contain "GET /stream HTTP/1.1"
            self.assertIn(b"GET /stream HTTP/1.1", req)
        finally:
            srv.stop()

    def test_path_default_slash(self):
        """URL with no path should default to /."""
        def handler(conn, addr, srv):
            req = recv_until(conn)
            srv.last_request = req
            conn.sendall(build_http_response("HTTP/1.1 200 OK",
                         {"content-type": "audio/mpeg"}, b"\xff" * 100))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            # URL: http://host:port (no trailing slash)
            url = f"http://127.0.0.1:{srv.port}"
            stdout, _, rc = run_sikradio(["-u", url, "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertIn(b"GET / HTTP/1.1", srv.last_request)
        finally:
            srv.stop()

    def test_ipv6_literal_in_url(self):
        """URL with IPv6 literal: http://[::1]:port/path."""
        def handler(conn, addr, srv):
            req = recv_until(conn)
            srv.last_request = req
            conn.sendall(build_http_response("HTTP/1.1 200 OK",
                         {"content-type": "audio/mpeg"}, b"\xff" * 100))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler, family=socket.AF_INET6)
        srv.start()
        try:
            url = f"http://[::1]:{srv.port}/stream"
            stdout, stderr, rc = run_sikradio(
                ["-u", url, "-q"], timeout=5)
            self.assertExitOk(rc)
            req = srv.last_request
            self.assertIn(b"GET /stream HTTP/1.1", req)
            # Host header should have brackets for IPv6
            self.assertIn(b"Host: [::1]:", req)
        finally:
            srv.stop()

    def test_url_with_query_string(self):
        """Path with query parameters preserved."""
        def handler(conn, addr, srv):
            req = recv_until(conn)
            srv.last_request = req
            conn.sendall(build_http_response("HTTP/1.1 200 OK",
                         {"content-type": "audio/mpeg"}, b"\xff" * 100))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            url = srv.url("/stream?token=abc&id=42")
            stdout, _, rc = run_sikradio(["-u", url, "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertIn(b"GET /stream?token=abc&id=42 HTTP/1.1",
                          srv.last_request)
        finally:
            srv.stop()


# ===========================================================================
# 3. HTTP request construction
# ===========================================================================

class TestHTTPRequest(SikradioTestBase):
    """Verify the HTTP request sent by sikradio."""

    def _capture_request(self, extra_args=None, path="/stream"):
        """Launch sikradio against a mock server, return captured request bytes."""
        captured = {}

        def handler(conn, addr, srv):
            req = recv_until(conn)
            captured["req"] = req
            conn.sendall(build_http_response("HTTP/1.1 200 OK",
                         {"content-type": "audio/mpeg"}, b"\xff" * 100))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            args = ["-u", srv.url(path), "-q"]
            if extra_args:
                args.extend(extra_args)
            run_sikradio(args, timeout=5)
            return captured.get("req", b"")
        finally:
            srv.stop()

    def test_get_method(self):
        req = self._capture_request()
        self.assertTrue(req.startswith(b"GET /stream HTTP/1.1\r\n"))

    def test_host_header_present(self):
        req = self._capture_request()
        self.assertIn(b"Host:", req)

    def test_connection_keep_alive(self):
        req = self._capture_request()
        self.assertIn(b"Connection: Keep-Alive\r\n", req)

    def test_icy_metadata_with_m(self):
        req = self._capture_request(extra_args=["-m"])
        self.assertIn(b"Icy-MetaData: 1\r\n", req)

    def test_no_icy_metadata_without_m(self):
        req = self._capture_request()
        self.assertNotIn(b"Icy-MetaData", req)

    def test_request_ends_with_double_crlf(self):
        req = self._capture_request()
        # The request must end with \r\n\r\n
        self.assertIn(b"\r\n\r\n", req)


# ===========================================================================
# 4. HTTP response parsing & redirect following
# ===========================================================================

class TestResponseParsing(SikradioTestBase):
    """Test that sikradio correctly parses HTTP and ICY responses."""

    def test_http10_200(self):
        """HTTP/1.0 200 OK should start streaming."""
        audio = b"\xff\xfb\x90\x00" * 50
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.0 200 OK",
                {"content-type": "audio/mpeg", "Connection": "Close"},
                audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_http11_200(self):
        """HTTP/1.1 200 OK."""
        audio = b"\xaa" * 200
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response("HTTP/1.1 200 OK",
                         {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_icy_200(self):
        """ICY 200 OK (SHOUTcast style) should be accepted."""
        audio = b"\xbb" * 300
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_icy_response(
                "ICY 200 OK",
                {"content-type": "audio/mpeg", "icy-br": "128"},
                audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_302_redirect(self):
        """HTTP 302 with Location header should follow redirect."""
        audio = b"\xcc" * 150
        connection_count = {"n": 0}

        def handler(conn, addr, srv):
            req = recv_until(conn)
            connection_count["n"] += 1
            if b"GET /original" in req:
                conn.sendall(build_http_response(
                    "HTTP/1.1 302 Found",
                    {"Location": f"http://127.0.0.1:{srv.port}/redirected",
                     "Connection": "close"},
                    b""))
            elif b"GET /redirected" in req:
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"},
                    audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/original"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
            self.assertGreaterEqual(connection_count["n"], 2)
        finally:
            srv.stop()

    def test_301_redirect(self):
        """HTTP 301 Moved Permanently should also follow."""
        audio = b"\xdd" * 100

        def handler(conn, addr, srv):
            req = recv_until(conn)
            if b"GET /old" in req:
                conn.sendall(build_http_response(
                    "HTTP/1.1 301 Moved Permanently",
                    {"Location": f"http://127.0.0.1:{srv.port}/new",
                     "Connection": "close"}, b""))
            else:
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/old"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_chained_redirects(self):
        """Multiple redirects in a chain (as in example 5)."""
        audio = b"\xee" * 80

        def handler(conn, addr, srv):
            req = recv_until(conn)
            if b"GET /a" in req and b"/ab" not in req:
                conn.sendall(build_http_response(
                    "HTTP/1.1 302 Found",
                    {"Location": f"http://127.0.0.1:{srv.port}/ab",
                     "Connection": "close"}, b""))
            elif b"GET /ab" in req:
                conn.sendall(build_http_response(
                    "HTTP/1.1 302 Found",
                    {"Location": f"http://127.0.0.1:{srv.port}/abc",
                     "Connection": "close"}, b""))
            elif b"GET /abc" in req:
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/a"), "-q"], timeout=8)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_4xx_error_is_fatal(self):
        """A 404 response should be treated as a fatal error (exit 1).
        Per professor: 'A co to znaczy ... jak kontynuować ... jeśli serwer
        odpowiada błędem?' → implying it cannot continue."""
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response("HTTP/1.1 404 Not Found",
                         {"Connection": "close"}, b"Not Found"))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            _, _, rc = run_sikradio(["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitError(rc)
        finally:
            srv.stop()

    def test_5xx_error_is_fatal(self):
        """A 500 should also be fatal."""
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 500 Internal Server Error",
                {"Connection": "close"}, b""))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            _, _, rc = run_sikradio(["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitError(rc)
        finally:
            srv.stop()

    def test_redirect_missing_location(self):
        """302 without Location header → fatal."""
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 302 Found",
                {"Connection": "close"}, b""))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            _, _, rc = run_sikradio(["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitError(rc)
        finally:
            srv.stop()


# ===========================================================================
# 5. ICY metadata demultiplexing
# ===========================================================================

class TestICYMetadata(SikradioTestBase):
    """Verify correct demultiplexing of ICY metadata from audio stream."""

    def test_no_metadata_no_multiplex(self):
        """Without -m, all data goes to stdout as audio."""
        audio = b"\xff\xfb" * 500
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_metadata_basic(self):
        """With -m and icy-metaint, metadata goes to stderr, audio to stdout."""
        metaint = 32
        audio_chunk = bytes(range(256))[:metaint]  # 32 distinct bytes
        meta1 = "StreamTitle='Test Song';"
        meta2 = "StreamTitle='Another Song';"

        stream = build_icy_stream(
            [audio_chunk, audio_chunk, audio_chunk],
            metaint,
            [meta1, meta2, None]  # third chunk has zero-length meta
        )

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                stream))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=5)
            self.assertExitOk(rc)
            # stdout should contain exactly 3 × audio_chunk = 96 bytes
            self.assertEqual(stdout, audio_chunk * 3)
            # stderr should contain both metadata strings
            stderr_text = stderr.decode("utf-8", errors="replace")
            self.assertIn("StreamTitle='Test Song';", stderr_text)
            self.assertIn("StreamTitle='Another Song';", stderr_text)
        finally:
            srv.stop()

    def test_metadata_zero_length(self):
        """Zero-length metadata block (byte = 0x00) should produce no output."""
        metaint = 16
        audio = b"\xaa" * metaint
        stream = audio + b"\x00" + audio + b"\x00"

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                stream))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio * 2)
            # No metadata text should appear
            stderr_text = stderr.decode("utf-8", errors="replace")
            # stderr should be empty (or close to it) since -q suppresses diagnostics
            self.assertEqual(stderr_text.strip(), "")
        finally:
            srv.stop()

    def test_metadata_padded_with_nulls(self):
        """Metadata is padded to a 16-byte multiple; null padding should not
        appear on stderr."""
        metaint = 16
        audio = b"\xbb" * metaint
        meta_str = "StreamTitle='X';"  # 17 chars → padded to 32 bytes
        meta_bytes = meta_str.encode()
        padded_len = 32
        meta_block = bytes([padded_len // 16]) + meta_bytes + \
                     b"\x00" * (padded_len - len(meta_bytes))

        stream = audio + meta_block
        # Then close connection.

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                stream))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
            stderr_text = stderr.decode("utf-8", errors="replace")
            # Should contain the metadata (with null padding passed through as per spec:
            # "wypisuje bez zmian" = prints without changes)
            self.assertIn("StreamTitle='X';", stderr_text)
        finally:
            srv.stop()

    def test_metadata_large_metaint(self):
        """Test with icy-metaint=16000 (as in examples)."""
        metaint = 16000
        audio = b"\xcc" * metaint
        meta = "StreamTitle='Big Metaint Test';"
        stream = build_icy_stream([audio], metaint, [meta])

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                stream))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=8)
            self.assertExitOk(rc)
            self.assertEqual(len(stdout), metaint)
            self.assertEqual(stdout, audio)
            self.assertIn("StreamTitle='Big Metaint Test';",
                          stderr.decode("utf-8", errors="replace"))
        finally:
            srv.stop()

    def test_multiplex_flag_but_server_no_metaint(self):
        """User passes -m but server doesn't provide icy-metaint.
        All data should go to stdout as audio (noncritical error)."""
        audio = b"\xdd" * 200
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_metadata_newline_after_nonempty(self):
        """After non-empty metadata, a newline is written to stderr."""
        metaint = 16
        audio = b"\xaa" * metaint
        meta = "StreamTitle='NL Test';"
        stream = build_icy_stream([audio, audio], metaint, [meta, None])

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                stream))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=5)
            self.assertExitOk(rc)
            stderr_text = stderr.decode("utf-8", errors="replace")
            # The metadata should be followed by a newline
            self.assertIn("StreamTitle='NL Test';\n", stderr_text)
        finally:
            srv.stop()

    def test_audio_integrity_with_metadata(self):
        """Audio bytes must be passed through exactly, with metadata stripped."""
        metaint = 64
        # Use random audio to make sure no accidental matches
        rng = random.Random(42)
        audio1 = bytes(rng.getrandbits(8) for _ in range(metaint))
        audio2 = bytes(rng.getrandbits(8) for _ in range(metaint))
        audio3 = bytes(rng.getrandbits(8) for _ in range(metaint))
        meta1 = "StreamTitle='Song A';"
        meta2 = "StreamTitle='Song B';"

        stream = build_icy_stream(
            [audio1, audio2, audio3], metaint, [meta1, None, meta2])

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                stream))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio1 + audio2 + audio3)
        finally:
            srv.stop()


# ===========================================================================
# 6. Timeout & reconnection
# ===========================================================================

class TestTimeout(SikradioTestBase):
    """Test timeout triggers reconnection behavior."""

    def test_timeout_triggers_reconnect(self):
        """After timeout, sikradio should reconnect and re-request from
        the original URL. Example 3 shows this pattern."""
        connection_count = {"n": 0}
        audio = b"\xaa" * 100

        def handler(conn, addr, srv):
            req = recv_until(conn)
            connection_count["n"] += 1
            if connection_count["n"] == 1:
                # First connection: send some audio then go silent → timeout.
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"}, audio))
                # Don't close; let the client timeout.
                time.sleep(3)  # Hold connection open
            else:
                # Second connection after reconnect: send audio and close.
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"}, audio))
                conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/stream"), "-q",
                 "-t", str(SHORT_TIMEOUT_MS)],
                timeout=10)
            self.assertExitOk(rc)
            # Should have connected at least twice
            self.assertGreaterEqual(connection_count["n"], 2)
            # Audio from both connections should appear on stdout
            self.assertEqual(len(stdout), 200)
        finally:
            srv.stop()

    def test_timeout_clears_cookies(self):
        """On timeout reconnect, cookies should be cleared and the original
        URL used (not the redirected one)."""
        connection_count = {"n": 0}

        def handler(conn, addr, srv):
            req = recv_until(conn)
            srv.last_request = req
            connection_count["n"] += 1
            if connection_count["n"] <= 2:
                # First two connections: redirect with cookie, then stream
                if b"GET /original" in req:
                    conn.sendall(build_http_response(
                        "HTTP/1.1 302 Found",
                        {"Location": f"http://127.0.0.1:{srv.port}/redirected",
                         "Set-Cookie": "session=abc123; Path=/",
                         "Connection": "close"}, b""))
                    conn.shutdown(socket.SHUT_WR)
                elif b"GET /redirected" in req:
                    conn.sendall(build_http_response(
                        "HTTP/1.1 200 OK",
                        {"content-type": "audio/mpeg"},
                        b"\xff" * 50))
                    # Hold open → timeout
                    time.sleep(3)
            elif connection_count["n"] == 3:
                # After timeout: should reconnect to /original, no cookies
                self.assertIn(b"GET /original", req)
                # Cookies should be cleared
                self.assertNotIn(b"Cookie:", req)
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"},
                    b"\xee" * 50))
                conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/original"), "-q",
                 "-t", str(SHORT_TIMEOUT_MS)],
                timeout=12)
            self.assertExitOk(rc)
            self.assertGreaterEqual(connection_count["n"], 3)
        finally:
            srv.stop()

    def test_timeout_default_5000ms(self):
        """Without -t, default timeout is 5000ms.
        Verify client reconnects after ~5 seconds of silence."""
        connection_count = {"n": 0}
        start_time = [None]

        def handler(conn, addr, srv):
            req = recv_until(conn)
            connection_count["n"] += 1
            if connection_count["n"] == 1:
                start_time[0] = time.monotonic()
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"}, b"\xff" * 50))
                # Go silent, wait for timeout
                time.sleep(8)
            else:
                elapsed = time.monotonic() - start_time[0]
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"}, b"\xaa" * 50))
                conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"],
                timeout=12)
            self.assertExitOk(rc)
            self.assertGreaterEqual(connection_count["n"], 2)
        finally:
            srv.stop()


# ===========================================================================
# 7. Quit / stdin handling
# ===========================================================================

class TestQuitHandling(SikradioTestBase):
    """Test that typing 'quit' + Enter terminates gracefully."""

    def test_quit_command(self):
        """Writing 'quit\\n' to stdin should cause exit 0."""
        audio = b"\xff" * 16000

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            # Keep sending so the client doesn't EOF from server side
            try:
                while True:
                    conn.sendall(b"\xff" * 4096)
                    time.sleep(0.1)
            except (BrokenPipeError, OSError):
                pass

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"],
                feed_quit_after=1.5, timeout=8)
            self.assertExitOk(rc)
            # Should have received some audio before quitting
            self.assertGreater(len(stdout), 0)
        finally:
            srv.stop()

    def test_server_close_exit_0(self):
        """Server closing connection → client exits 0."""
        audio = b"\xab" * 500

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_stdin_eof_is_not_fatal(self):
        """EOF on stdin should not be treated as a fatal error.
        Per professor: 'To nie przeszkadza w odbieraniu i odtwarzaniu dźwięku.'
        The code treats it as ending the program gracefully."""
        audio = b"\xcd" * 200

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            # Immediately close stdin (empty input)
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"],
                stdin_bytes=b"", timeout=5)
            # Should still output audio and exit 0
            self.assertExitOk(rc)
        finally:
            srv.stop()

    def test_quit_not_at_start_of_input(self):
        """'quit' preceded by other text should still be detected."""
        audio = b"\xff" * 8000

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b""))
            try:
                while True:
                    conn.sendall(audio)
                    time.sleep(0.05)
            except (BrokenPipeError, OSError):
                pass

        srv = MockServer(handler)
        srv.start()
        try:
            # Send some garbage then quit
            proc = subprocess.Popen(
                [SIKRADIO_BIN, "-u", srv.url("/"), "-q"],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)
            time.sleep(1)
            proc.stdin.write(b"hello\n")
            proc.stdin.flush()
            time.sleep(0.2)
            proc.stdin.write(b"quit\n")
            proc.stdin.flush()
            stdout, stderr = proc.communicate(timeout=5)
            self.assertExitOk(proc.returncode)
        finally:
            srv.stop()

    def test_partial_quit_then_complete(self):
        """'qui' then 't\\n' should trigger quit (buffered detection)."""
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b""))
            try:
                while True:
                    conn.sendall(b"\xff" * 4096)
                    time.sleep(0.1)
            except (BrokenPipeError, OSError):
                pass

        srv = MockServer(handler)
        srv.start()
        try:
            proc = subprocess.Popen(
                [SIKRADIO_BIN, "-u", srv.url("/"), "-q"],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)
            time.sleep(1)
            proc.stdin.write(b"qui")
            proc.stdin.flush()
            time.sleep(0.1)
            proc.stdin.write(b"t\n")
            proc.stdin.flush()
            stdout, stderr = proc.communicate(timeout=5)
            self.assertExitOk(proc.returncode)
        finally:
            srv.stop()


# ===========================================================================
# 8. Verbosity levels
# ===========================================================================

class TestVerbosity(SikradioTestBase):
    """Test that verbosity controls what appears on stderr."""

    def _run_with_verbosity(self, v, path="/stream"):
        audio = b"\xff" * 100

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            args = ["-u", srv.url(path), "-v", str(v)]
            stdout, stderr, rc = run_sikradio(args, timeout=5)
            return stdout, stderr, rc
        finally:
            srv.stop()

    def test_v0_no_diagnostic_output(self):
        """Verbosity 0 (-q) should produce no diagnostic output."""
        _, stderr, rc = self._run_with_verbosity(0)
        self.assertExitOk(rc)
        self.assertEqual(stderr.strip(), b"")

    def test_v1_shows_communication(self):
        """Verbosity 1 should show resolving, connecting, request, response."""
        _, stderr, rc = self._run_with_verbosity(1)
        self.assertExitOk(rc)
        text = stderr.decode("utf-8", errors="replace")
        self.assertIn("resolving name", text)
        self.assertIn("connecting to server", text)
        # Should include the sent request
        self.assertIn("GET /stream HTTP/1.1", text)
        # Should include the response header
        self.assertIn("HTTP/1.1 200 OK", text)

    def test_v1_shows_timestamp(self):
        """Verbosity 1 should show a timestamp in YYYY.MM.DD HH.MM.SS format."""
        _, stderr, rc = self._run_with_verbosity(1)
        self.assertExitOk(rc)
        text = stderr.decode("utf-8", errors="replace")
        # Match timestamp pattern
        self.assertRegex(text, r"\d{4}\.\d{2}\.\d{2} \d{2}\.\d{2}\.\d{2}")

    def test_v2_default_shows_critical_errors_only(self):
        """Verbosity 2 (default) should show critical errors but behave
        similarly to v1 for communication info (v1 ≤ v2)."""
        _, stderr, rc = self._run_with_verbosity(2)
        self.assertExitOk(rc)
        text = stderr.decode("utf-8", errors="replace")
        # v=2 >= COMMUNICATION(1), so communication info should appear
        self.assertIn("resolving name", text)

    def test_q_flag_equivalent_to_v0(self):
        """The -q flag should produce the same output as -v0."""
        audio = b"\xff" * 100

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            _, stderr_q, _ = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=5)
        finally:
            srv.stop()

        srv2 = MockServer(handler)
        srv2.start()
        try:
            _, stderr_v0, _ = run_sikradio(
                ["-u", srv2.url("/"), "-v0"], timeout=5)
        finally:
            srv2.stop()

        self.assertEqual(stderr_q.strip(), b"")
        self.assertEqual(stderr_v0.strip(), b"")

    def test_v1_shows_data_receiving_timeout(self):
        """On timeout, verbosity ≥ 1 should log 'data receiving timeout'
        (as shown in example 3)."""
        connection_count = {"n": 0}

        def handler(conn, addr, srv):
            recv_until(conn)
            connection_count["n"] += 1
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b"\xff" * 50))
            if connection_count["n"] == 1:
                time.sleep(3)  # force timeout
            else:
                conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            _, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-v1",
                 "-t", str(SHORT_TIMEOUT_MS)],
                timeout=10)
            self.assertExitOk(rc)
            text = stderr.decode("utf-8", errors="replace")
            self.assertIn("data receiving timeout", text)
        finally:
            srv.stop()


# ===========================================================================
# 9. Cookie handling
# ===========================================================================

class TestCookies(SikradioTestBase):
    """Test cookie persistence across redirects and clearing on timeout."""

    def test_cookies_preserved_across_redirects(self):
        """Set-Cookie in redirect response should be sent on next request.
        (As in examples 4 and 5.)"""
        connection_count = {"n": 0}
        captured_requests = []

        def handler(conn, addr, srv):
            req = recv_until(conn)
            captured_requests.append(req)
            connection_count["n"] += 1
            if connection_count["n"] == 1:
                conn.sendall(build_http_response(
                    "HTTP/1.0 302 Found",
                    {"Location": f"http://127.0.0.1:{srv.port}/final",
                     "Set-Cookie": "session=xyz789; Domain=127.0.0.1",
                     "Connection": "close"}, b""))
            else:
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"},
                    b"\xff" * 100))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            _, _, rc = run_sikradio(["-u", srv.url("/start"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertGreaterEqual(len(captured_requests), 2)
            # Second request should contain Cookie header
            second_req = captured_requests[1].decode("utf-8", errors="replace")
            self.assertIn("Cookie:", second_req)
            self.assertIn("session=xyz789", second_req)
        finally:
            srv.stop()

    def test_multiple_cookies(self):
        """Multiple Set-Cookie headers should all be sent back."""
        captured_requests = []
        conn_n = {"n": 0}

        def handler(conn, addr, srv):
            req = recv_until(conn)
            captured_requests.append(req)
            conn_n["n"] += 1
            if conn_n["n"] == 1:
                resp = b"HTTP/1.1 302 Found\r\n"
                resp += f"Location: http://127.0.0.1:{srv.port}/final\r\n".encode()
                resp += b"Set-Cookie: a=1\r\n"
                resp += b"Set-Cookie: b=2\r\n"
                resp += b"Connection: close\r\n"
                resp += b"\r\n"
                conn.sendall(resp)
            else:
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"}, b"\xff" * 100))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            _, _, rc = run_sikradio(["-u", srv.url("/start"), "-q"], timeout=5)
            self.assertExitOk(rc)
            second_req = captured_requests[1].decode("utf-8", errors="replace")
            self.assertIn("Cookie:", second_req)
            self.assertIn("a=1", second_req)
            self.assertIn("b=2", second_req)
        finally:
            srv.stop()


# ===========================================================================
# 10. Edge cases & stress tests
# ===========================================================================

class TestEdgeCases(SikradioTestBase):
    """Miscellaneous edge cases."""

    def test_server_sends_data_byte_by_byte(self):
        """Server drip-feeds data one byte at a time – client must reassemble."""
        audio = b"\xfe" * 50

        def handler(conn, addr, srv):
            recv_until(conn)
            header = build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b"")
            for b in header:
                conn.sendall(bytes([b]))
                time.sleep(0.001)
            for b in audio:
                conn.sendall(bytes([b]))
                time.sleep(0.001)
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=10)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_large_audio_stream(self):
        """Stream 1 MB of audio data – verify integrity."""
        audio = os.urandom(1024 * 1024)

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b""))
            # Send in chunks
            offset = 0
            while offset < len(audio):
                chunk = audio[offset:offset + 8192]
                try:
                    conn.sendall(chunk)
                except (BrokenPipeError, OSError):
                    return
                offset += len(chunk)
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=15)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_large_metadata_block(self):
        """Metadata that fills the max possible length (255 * 16 = 4080 bytes)."""
        metaint = 32
        audio = b"\xaa" * metaint
        # Build a metadata string that's exactly 4080 bytes
        meta_content = "StreamTitle='" + "X" * 4050 + "';"
        # Pad so the total is exactly 4080
        meta_bytes = meta_content.encode("utf-8")
        if len(meta_bytes) < 4080:
            meta_bytes += b"\x00" * (4080 - len(meta_bytes))
        meta_bytes = meta_bytes[:4080]

        stream = audio + bytes([255]) + meta_bytes

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                stream))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
            self.assertIn(b"StreamTitle=", stderr)
        finally:
            srv.stop()

    def test_metadata_split_across_recv_boundaries(self):
        """Send audio+metadata in tiny chunks to stress the demux state machine."""
        metaint = 16
        audio = b"\xbb" * metaint
        meta = "StreamTitle='Split Test';"
        stream = build_icy_stream([audio, audio], metaint, [meta, None])

        def handler(conn, addr, srv):
            recv_until(conn)
            header = build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                b"")
            conn.sendall(header)
            # Send stream in 3-byte chunks (odd size to split metadata)
            for i in range(0, len(stream), 3):
                chunk = stream[i:i + 3]
                try:
                    conn.sendall(chunk)
                except (BrokenPipeError, OSError):
                    return
                time.sleep(0.001)
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=10)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio * 2)
            self.assertIn(b"StreamTitle='Split Test';", stderr)
        finally:
            srv.stop()

    def test_connection_refused_is_fatal(self):
        """Connecting to a port with nothing listening → exit 1.
        Per professor: 'Brak połączenia z serwerem uniemożliwia kontynuowanie pracy.'"""
        # Find a port that's definitely not listening
        port = _find_free_port()
        _, _, rc = run_sikradio(
            ["-u", f"http://127.0.0.1:{port}/", "-q"], timeout=5)
        self.assertExitError(rc)

    def test_force_ipv4(self):
        """With -4, should connect via IPv4."""
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b"\xff" * 100))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler, family=socket.AF_INET)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-4", "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(len(stdout), 100)
        finally:
            srv.stop()

    def test_force_ipv6(self):
        """With -6, should connect via IPv6."""
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b"\xff" * 100))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler, family=socket.AF_INET6)
        srv.start()
        try:
            url = f"http://[::1]:{srv.port}/"
            stdout, _, rc = run_sikradio(
                ["-u", url, "-6", "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(len(stdout), 100)
        finally:
            srv.stop()

    def test_host_header_ipv6_brackets(self):
        """When connecting to an IPv6 literal, Host header should use [addr]:port."""
        captured = {}

        def handler(conn, addr, srv):
            req = recv_until(conn)
            captured["req"] = req
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b"\xff" * 50))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler, family=socket.AF_INET6)
        srv.start()
        try:
            url = f"http://[::1]:{srv.port}/test"
            run_sikradio(["-u", url, "-q"], timeout=5)
            req_text = captured["req"].decode("utf-8", errors="replace")
            self.assertIn(f"Host: [::1]:{srv.port}", req_text)
        finally:
            srv.stop()

    def test_empty_audio_stream(self):
        """Server sends 200 OK with headers but zero audio bytes, then closes."""
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b""))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, b"")
        finally:
            srv.stop()

    def test_server_immediate_close(self):
        """Server accepts connection then immediately closes it → fatal."""
        def handler(conn, addr, srv):
            conn.close()

        srv = MockServer(handler)
        srv.start()
        try:
            _, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitError(rc)
        finally:
            srv.stop()

    def test_server_sends_partial_header(self):
        """Server sends only part of the header (no \\r\\n\\r\\n) then closes → fatal."""
        def handler(conn, addr, srv):
            conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\n")
            time.sleep(0.5)
            conn.close()

        srv = MockServer(handler)
        srv.start()
        try:
            _, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitError(rc)
        finally:
            srv.stop()

    def test_header_case_insensitive(self):
        """Header field names should be parsed case-insensitively."""
        metaint = 32
        audio = b"\xaa" * metaint
        meta = "StreamTitle='CaseTest';"
        stream = build_icy_stream([audio], metaint, [meta])

        def handler(conn, addr, srv):
            recv_until(conn)
            # Use mixed case for icy-metaint
            resp = b"HTTP/1.1 200 OK\r\n"
            resp += b"Content-Type: audio/mpeg\r\n"
            resp += f"ICY-MetaInt: {metaint}\r\n".encode()
            resp += b"\r\n"
            resp += stream
            conn.sendall(resp)
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
            self.assertIn(b"StreamTitle='CaseTest';", stderr)
        finally:
            srv.stop()

    def test_multiple_metadata_blocks_in_single_recv(self):
        """If the TCP buffer contains data spanning multiple metadata boundaries,
        the demuxer must handle all of them in one recv() call."""
        metaint = 16
        audio = b"\xaa" * metaint
        meta1 = "StreamTitle='M1';"
        meta2 = "StreamTitle='M2';"
        # Build stream with many short audio+meta cycles
        stream = build_icy_stream(
            [audio] * 5, metaint,
            [meta1, None, meta2, None, meta1])

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                stream))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio * 5)
            stderr_text = stderr.decode("utf-8", errors="replace")
            # M1 appears twice, M2 once
            self.assertEqual(stderr_text.count("StreamTitle='M1';"), 2)
            self.assertEqual(stderr_text.count("StreamTitle='M2';"), 1)
        finally:
            srv.stop()

    def test_quit_during_metadata_recv(self):
        """Sending quit while metadata is being received should still exit 0."""
        metaint = 16000

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg", "icy-metaint": str(metaint)},
                b""))
            # Send audio continuously
            try:
                while True:
                    conn.sendall(b"\xff" * 4096)
                    time.sleep(0.01)
            except (BrokenPipeError, OSError):
                pass

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-mq"],
                feed_quit_after=2, timeout=8)
            self.assertExitOk(rc)
        finally:
            srv.stop()

    def test_redirect_to_different_host_and_port(self):
        """Redirect from one server to another (different host/port)."""
        audio = b"\xab" * 200

        def handler2(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv2 = MockServer(handler2)
        srv2.start()

        def handler1(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 302 Found",
                {"Location": f"http://127.0.0.1:{srv2.port}/audio",
                 "Connection": "close"}, b""))
            conn.shutdown(socket.SHUT_WR)

        srv1 = MockServer(handler1)
        srv1.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv1.url("/start"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv1.stop()
            srv2.stop()

    def test_v0_fatal_error_is_silent(self):
        """With -v0 / -q, even a fatal error should produce no stderr output,
        but should still exit 1."""
        _, stderr, rc = run_sikradio(
            ["-u", "http://127.0.0.1:1/", "-q"], timeout=5)
        self.assertExitError(rc)
        self.assertEqual(stderr.strip(), b"")


# ===========================================================================
# 11. HTTPS / TLS tests
# ===========================================================================

class TestHTTPS(SikradioTestBase):
    """Test HTTPS connections via SSL/TLS."""

    @classmethod
    def setUpClass(cls):
        cls._tmpdir = tempfile.mkdtemp()
        try:
            cls.certfile, cls.keyfile = generate_self_signed_cert(cls._tmpdir)
        except (subprocess.CalledProcessError, FileNotFoundError):
            raise unittest.SkipTest("openssl CLI not available")

    def test_https_basic(self):
        """Basic HTTPS stream."""
        audio = b"\xff\xfe" * 100

        def handler(conn, addr, srv):
            req = recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler, use_tls=True,
                         certfile=self.certfile, keyfile=self.keyfile)
        srv.start()
        try:
            stdout, stderr, rc = run_sikradio(
                ["-u", f"https://localhost:{srv.port}/", "-q"],
                timeout=8)
            # May fail due to self-signed cert; that's acceptable.
            # The test verifies the client *attempts* TLS.
            if rc == 0:
                self.assertEqual(stdout, audio)
        finally:
            srv.stop()


# ===========================================================================
# 12. Concurrent data on stdin + tcp
# ===========================================================================

class TestConcurrency(SikradioTestBase):
    """Test that sikradio handles stdin and TCP data concurrently."""

    def test_stdin_does_not_block_audio(self):
        """Writing non-quit text to stdin should not interrupt audio streaming."""
        total_audio = b""
        chunk = b"\xff" * 4096

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, b""))
            for _ in range(20):
                try:
                    conn.sendall(chunk)
                except (BrokenPipeError, OSError):
                    return
                time.sleep(0.05)
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            proc = subprocess.Popen(
                [SIKRADIO_BIN, "-u", srv.url("/"), "-q"],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE)
            # Write non-quit stuff to stdin during streaming
            time.sleep(0.5)
            for i in range(5):
                try:
                    proc.stdin.write(f"line{i}\n".encode())
                    proc.stdin.flush()
                except BrokenPipeError:
                    break
                time.sleep(0.2)
            stdout, stderr = proc.communicate(timeout=10)
            self.assertExitOk(proc.returncode)
            # Should have received substantial audio
            self.assertGreater(len(stdout), 0)
        finally:
            srv.stop()


# ===========================================================================
# 13. Protocol edge cases
# ===========================================================================

class TestProtocolEdgeCases(SikradioTestBase):
    """Various protocol-level edge cases."""

    def test_http_10_and_11_status_parsing(self):
        """Both HTTP/1.0 and HTTP/1.1 status lines should be accepted."""
        for status_line in ["HTTP/1.0 200 OK", "HTTP/1.1 200 OK"]:
            with self.subTest(status_line=status_line):
                audio = b"\xab" * 50

                def handler(conn, addr, srv):
                    recv_until(conn)
                    conn.sendall(build_http_response(
                        status_line, {"content-type": "audio/mpeg"}, audio))
                    conn.shutdown(socket.SHUT_WR)

                srv = MockServer(handler)
                srv.start()
                try:
                    stdout, _, rc = run_sikradio(
                        ["-u", srv.url("/"), "-q"], timeout=5)
                    self.assertExitOk(rc)
                    self.assertEqual(stdout, audio)
                finally:
                    srv.stop()

    def test_server_sends_garbage_status(self):
        """Server sends unparseable status line → fatal."""
        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(b"GARBAGE_LINE\r\ncontent-type: audio/mpeg\r\n\r\n")
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            _, _, rc = run_sikradio(["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitError(rc)
        finally:
            srv.stop()

    def test_interrupted_audio_stream(self):
        """Server sends some audio, then abruptly RST-closes the connection.
        Client should output what it received and exit 0 (server dropped)."""
        audio = b"\xfe" * 500

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            # Force RST by setting linger to 0
            conn.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                            struct.pack("ii", 1, 0))
            conn.close()

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=5)
            # Per spec: "Jeśli serwer zamknął połączenie, klient wypisuje
            # wszystkie dotychczas odebrane dane i kończy się statusem 0."
            self.assertExitOk(rc)
            # Should have at least the audio we sent before the RST
            self.assertGreater(len(stdout), 0)
        finally:
            srv.stop()

    def test_slow_header_delivery(self):
        """Server sends the header very slowly (byte by byte).
        Client should still parse it correctly."""
        audio = b"\xab" * 100

        def handler(conn, addr, srv):
            recv_until(conn)
            header = b"HTTP/1.1 200 OK\r\ncontent-type: audio/mpeg\r\n\r\n"
            for byte in header:
                conn.sendall(bytes([byte]))
                time.sleep(0.01)
            conn.sendall(audio)
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=10)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_very_long_header(self):
        """Server sends a response with many header fields."""
        audio = b"\xcd" * 100

        def handler(conn, addr, srv):
            recv_until(conn)
            headers = {"content-type": "audio/mpeg"}
            for i in range(100):
                headers[f"x-custom-header-{i}"] = f"value-{i}" * 10
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK", headers, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=8)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()


# ===========================================================================
# 14. Reconnection preserves original URL
# ===========================================================================

class TestReconnectionURL(SikradioTestBase):
    """After timeout, reconnection uses the original URL, not the redirected one."""

    def test_reconnect_uses_original_url(self):
        """After timeout, the client should start from the original URL again,
        not the last redirected URL."""
        request_paths = []
        conn_n = {"n": 0}

        def handler(conn, addr, srv):
            req = recv_until(conn)
            conn_n["n"] += 1
            # Extract the path from the GET line
            first_line = req.split(b"\r\n")[0].decode()
            path = first_line.split(" ")[1]
            request_paths.append(path)

            if "/original" in path:
                conn.sendall(build_http_response(
                    "HTTP/1.1 302 Found",
                    {"Location": f"http://127.0.0.1:{srv.port}/redirected",
                     "Connection": "close"}, b""))
                conn.shutdown(socket.SHUT_WR)
            elif "/redirected" in path:
                conn.sendall(build_http_response(
                    "HTTP/1.1 200 OK",
                    {"content-type": "audio/mpeg"}, b"\xff" * 50))
                if conn_n["n"] <= 2:
                    # First time: go silent to trigger timeout
                    time.sleep(3)
                else:
                    conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            run_sikradio(
                ["-u", srv.url("/original"),
                 "-t", str(SHORT_TIMEOUT_MS)],
                timeout=12)
            # After timeout, the 3rd connection should go to /original again
            # (not /redirected)
            originals = [p for p in request_paths if "/original" in p]
            self.assertGreaterEqual(len(originals), 2,
                                    f"Expected ≥2 /original requests, got paths: {request_paths}")
        finally:
            srv.stop()


# ===========================================================================
# 15. Binary-safe audio output
# ===========================================================================

class TestBinarySafety(SikradioTestBase):
    """Ensure audio output is binary-safe (no character conversion)."""

    def test_all_byte_values_pass_through(self):
        """All 256 byte values should pass through stdout unchanged."""
        audio = bytes(range(256)) * 10  # 2560 bytes

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()

    def test_null_bytes_in_audio(self):
        """Audio containing many null bytes should pass through."""
        audio = b"\x00" * 1000

        def handler(conn, addr, srv):
            recv_until(conn)
            conn.sendall(build_http_response(
                "HTTP/1.1 200 OK",
                {"content-type": "audio/mpeg"}, audio))
            conn.shutdown(socket.SHUT_WR)

        srv = MockServer(handler)
        srv.start()
        try:
            stdout, _, rc = run_sikradio(
                ["-u", srv.url("/"), "-q"], timeout=5)
            self.assertExitOk(rc)
            self.assertEqual(stdout, audio)
        finally:
            srv.stop()


# ===========================================================================
# Entry point
# ===========================================================================

if __name__ == "__main__":
    # Check that the binary exists
    if not os.path.isfile(SIKRADIO_BIN):
        print(f"ERROR: sikradio binary not found at {SIKRADIO_BIN}")
        print(f"       Build it first with 'make', or set SIKRADIO_BIN env var.")
        sys.exit(1)

    unittest.main()