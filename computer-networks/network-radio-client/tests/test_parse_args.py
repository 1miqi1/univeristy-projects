import ctypes
import pytest
import os

# =========================
# LOAD C++ LIBRARY
# =========================

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
LIB = os.path.join(ROOT, "libargs.so")

lib = ctypes.CDLL(LIB)

class Options(ctypes.Structure):
    _fields_ = [
        ("url", ctypes.c_char_p),
        ("multiplex", ctypes.c_bool),
        ("timeout_ms", ctypes.c_int),
        ("force_ipv4", ctypes.c_bool),
        ("force_ipv6", ctypes.c_bool),
        ("verbosity", ctypes.c_int),
        ("scheme", ctypes.c_char_p),
        ("host", ctypes.c_char_p),
        ("path", ctypes.c_char_p),
        ("port", ctypes.c_uint16),
    ]

lib.parse_args.argtypes = [
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.POINTER(Options),
    ctypes.POINTER(ctypes.c_char_p)
]
lib.parse_args.restype = ctypes.c_bool


# =========================
# HELPER
# =========================

def run_args(args):
    argc = len(args)
    argv = (ctypes.c_char_p * argc)(*(a.encode() for a in args))

    opt = Options()
    error = ctypes.c_char_p()

    ok = lib.parse_args(argc, argv, ctypes.byref(opt), ctypes.byref(error))

    err_str = error.value.decode() if error.value else ""
    return ok, opt, err_str


# =========================
# BASIC TESTS
# =========================

def test_minimal_valid():
    ok, opt, err = run_args(["prog", "-u", "http://example.com"])
    assert ok
    assert "example.com" in opt.url.decode()


def test_full_basic_config():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-t", "5000",
        "-v", "3",
        "-m"
    ])

    assert ok
    assert opt.timeout_ms == 5000
    assert opt.verbosity == 3
    assert opt.multiplex is True


# =========================
# FLAGS TESTS
# =========================

def test_grouped_flags_m46():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-m46"
    ])

    assert ok
    assert opt.multiplex is True
    assert opt.force_ipv4 is True
    assert opt.force_ipv6 is True


def test_ipv4_only():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-4"
    ])

    assert ok
    assert opt.force_ipv4 is True
    assert opt.force_ipv6 is False


def test_ipv6_only():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-6"
    ])

    assert ok
    assert opt.force_ipv6 is True


# =========================
# VALUE TESTS
# =========================

def test_timeout_valid():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-t", "100"
    ])

    assert ok
    assert opt.timeout_ms == 100


def test_timeout_invalid_low():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-t", "50"
    ])

    assert not ok


def test_timeout_invalid_high():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-t", "999999"
    ])

    assert not ok


def test_verbosity_min_max():
    for v in ["0", "1", "2", "3", "4"]:
        ok, opt, err = run_args([
            "prog",
            "-u", "http://example.com",
            "-v", v
        ])
        assert ok


def test_invalid_verbosity():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-v", "9"
    ])

    assert not ok


# =========================
# DUPLICATES (LAST WINS)
# =========================

def test_duplicate_verbosity_last_wins():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-v", "1",
        "-v", "4"
    ])

    assert ok
    assert opt.verbosity == 4


def test_duplicate_timeout_last_wins():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-t", "1000",
        "-t", "5000"
    ])

    assert ok
    assert opt.timeout_ms == 5000


# =========================
# ERROR CASES
# =========================

def test_missing_url():
    ok, opt, err = run_args(["prog", "-t", "500"])
    assert not ok


def test_unknown_flag():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-x"
    ])

    assert not ok


def test_missing_value_for_t():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-t"
    ])

    assert not ok


def test_missing_value_for_v():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-v"
    ])

    assert not ok


# =========================
# EDGE CASES
# =========================

def test_order_independence():
    ok, opt, err = run_args([
        "prog",
        "-v", "2",
        "-u", "http://example.com",
        "-t", "2000",
        "-m"
    ])

    assert ok


def test_blocked_flags_and_values():
    ok, opt, err = run_args([
        "prog",
        "-m46",
        "-u", "http://example.com"
    ])

    assert ok
    assert opt.multiplex
    assert opt.force_ipv4
    assert opt.force_ipv6


def test_overwrite_mixed():
    ok, opt, err = run_args([
        "prog",
        "-u", "http://example.com",
        "-v", "1",
        "-v", "2",
        "-v", "3"
    ])

    assert ok
    assert opt.verbosity == 3