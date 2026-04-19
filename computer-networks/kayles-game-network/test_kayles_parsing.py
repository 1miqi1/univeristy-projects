import subprocess
import unittest
from dataclasses import dataclass
from typing import Optional

from colorama import Fore, init

init(autoreset=True)

CLIENT_BIN = "./kayles_client"
SERVER_BIN = "./kayles_server"


@dataclass
class Case:
    name: str
    program: str
    args: list[str]
    expected_code: Optional[int] = None
    stderr_contains: Optional[str] = None
    stdout_contains: Optional[str] = None
    expected_behavior: str = "exit"   # "exit", "timeout", "either"


def run_case(case: Case) -> tuple[bool, str]:
    try:
        result = subprocess.run(
            [case.program] + case.args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=1.5
        )

        if case.expected_behavior == "timeout":
            return (
                False,
                f"expected process to keep running, but it exited with code {result.returncode}\n"
                f"stdout: {result.stdout!r}\n"
                f"stderr: {result.stderr!r}"
            )

        if case.expected_code is not None and result.returncode != case.expected_code:
            return (
                False,
                f"wrong exit code\n"
                f"expected: {case.expected_code}\n"
                f"got     : {result.returncode}\n"
                f"stdout  : {result.stdout!r}\n"
                f"stderr  : {result.stderr!r}"
            )

        if case.stderr_contains is not None and case.stderr_contains not in result.stderr:
            return (
                False,
                f"stderr does not contain expected text\n"
                f"expected fragment: {case.stderr_contains!r}\n"
                f"stderr           : {result.stderr!r}"
            )

        if case.stdout_contains is not None and case.stdout_contains not in result.stdout:
            return (
                False,
                f"stdout does not contain expected text\n"
                f"expected fragment: {case.stdout_contains!r}\n"
                f"stdout           : {result.stdout!r}"
            )

        return True, "ok"

    except subprocess.TimeoutExpired as e:
        if case.expected_behavior in ("timeout", "either"):
            return True, "ok"

        return (
            False,
            f"process timed out unexpectedly\n"
            f"partial stdout: {e.stdout!r}\n"
            f"partial stderr: {e.stderr!r}"
        )


def client_cases() -> list[Case]:
    return [
        Case(
            name="client: missing all args",
            program=CLIENT_BIN,
            args=[],
            expected_code=1,
            stderr_contains="usage:"
        ),
        Case(
            name="client: missing timeout option",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "0/1"],
            expected_code=1,
            stderr_contains="usage:"
        ),
        Case(
            name="client: unknown option",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "0/1", "-x", "5"],
            expected_code=1,
            stderr_contains="unknown option"
        ),
        Case(
            name="client: wrong option format",
            program=CLIENT_BIN,
            args=["--p", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="client: option too long (-pp)",
            program=CLIENT_BIN,
            args=["-pp", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="client: option too long (-abc)",
            program=CLIENT_BIN,
            args=["-abc", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="client: single dash (-)",
            program=CLIENT_BIN,
            args=["-", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="client: missing dash (p)",
            program=CLIENT_BIN,
            args=["p", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="client: triple dash (---)",
            program=CLIENT_BIN,
            args=["---", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="client: port zero invalid",
            program=CLIENT_BIN,
            args=["-p", "0", "-a", "127.0.0.1", "-m", "0/1", "-t", "5"],
            expected_code=1,
            stderr_contains="client port must be in range 1-65535"
        ),
        Case(
            name="client: invalid port text",
            program=CLIENT_BIN,
            args=["-p", "abc", "-a", "127.0.0.1", "-m", "0/1", "-t", "5"],
            expected_code=1,
            stderr_contains="not a valid port number"
        ),
        Case(
            name="client: invalid timeout format",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "abc"],
            expected_code=1,
            stderr_contains="wrong format of client timeout"
        ),
        Case(
            name="client: timeout zero invalid",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "0"],
            expected_code=1,
            stderr_contains="client timeout out of range"
        ),
        Case(
            name="client: timeout too large",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "100"],
            expected_code=1,
            stderr_contains="client timeout out of range"
        ),
        Case(
            name="client: empty message",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong client message"
        ),
        Case(
            name="client: message non-numeric field",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "0/a", "-t", "5"],
            expected_code=1,
            stderr_contains="Fields must be non-negative integers"
        ),
        Case(
            name="client: unknown message type",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "9/1", "-t", "5"],
            expected_code=1,
            stderr_contains="Unknown message type"
        ),
        Case(
            name="client: player id zero",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "0/0", "-t", "5"],
            expected_code=1,
            stderr_contains="Player index can't be 0"
        ),
        Case(
            name="client: join wrong field count",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "0/1/2", "-t", "5"],
            expected_code=1,
            stderr_contains="JOIN requires exactly"
        ),
        Case(
            name="client: move missing fields",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "1/1/2", "-t", "5"],
            expected_code=1,
            stderr_contains="MOVE requires"
        ),
        Case(
            name="client: move pawn too large",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "1/1/2/999", "-t", "5"],
            expected_code=1,
            stderr_contains="Pawn index out of range"
        ),
        Case(
            name="client: utility message wrong field count",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "3/1", "-t", "5"],
            expected_code=1,
            stderr_contains="Utility msg requires"
        ),
        Case(
            name="client: last port wins",
            program=CLIENT_BIN,
            args=["-p", "0", "-p", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "1"],
            expected_behavior="either",
            expected_code=0
        ),
        Case(
            name="client: last timeout wins",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "0/1", "-t", "200", "-t", "1"],
            expected_behavior="either",
            expected_code=0
        ),
        Case(
            name="client: last message wins",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "127.0.0.1", "-m", "9/1", "-m", "0/1", "-t", "1"],
            expected_behavior="either",
            expected_code=0
        ),
        Case(
            name="client: last address wins",
            program=CLIENT_BIN,
            args=["-p", "1234", "-a", "bad.invalid", "-a", "127.0.0.1", "-m", "0/1", "-t", "1"],
            expected_behavior="either",
            expected_code=0
        ),
    ]


def server_cases() -> list[Case]:
    too_long_row = "1" * 257

    return [
        Case(
            name="server: missing all args",
            program=SERVER_BIN,
            args=[],
            expected_code=1,
            stderr_contains="usage:"
        ),
        Case(
            name="server: missing timeout option",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "127.0.0.1", "-p", "1234"],
            expected_code=1,
            stderr_contains="usage:"
        ),
        Case(
            name="server: unknown option",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "127.0.0.1", "-p", "1234", "-x", "5"],
            expected_code=1,
            stderr_contains="unknown option"
        ),
        Case(
            name="server: wrong option format",
            program=SERVER_BIN,
            args=["--r", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="server: option too long (-pp)",
            program=SERVER_BIN,
            args=["-pp", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="server: option too long (-abc)",
            program=SERVER_BIN,
            args=["-abc", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="server: single dash (-)",
            program=SERVER_BIN,
            args=["-", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="server: missing dash (r)",
            program=SERVER_BIN,
            args=["r", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="server: triple dash (---)",
            program=SERVER_BIN,
            args=["---", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong option format"
        ),
        Case(
            name="server: invalid port text",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "127.0.0.1", "-p", "abc", "-t", "5"],
            expected_code=1,
            stderr_contains="not a valid port number"
        ),
        Case(
            name="server: port 0 allowed",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "127.0.0.1", "-p", "0", "-t", "1"],
            expected_behavior="timeout"
        ),
        Case(
            name="server: invalid timeout format",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "abc"],
            expected_code=1,
            stderr_contains="wrong format of server timeout"
        ),
        Case(
            name="server: timeout zero invalid",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "0"],
            expected_code=1,
            stderr_contains="server timeout out of range"
        ),
        Case(
            name="server: timeout too large",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "100"],
            expected_code=1,
            stderr_contains="server timeout out of range"
        ),
        Case(
            name="server: empty pawn row",
            program=SERVER_BIN,
            args=["-r", "", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="pawn row length out of range"
        ),
        Case(
            name="server: pawn row too long",
            program=SERVER_BIN,
            args=["-r", too_long_row, "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="pawn row length out of range"
        ),
        Case(
            name="server: pawn row invalid char",
            program=SERVER_BIN,
            args=["-r", "10a1", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="wrong format of pawn row"
        ),
        Case(
            name="server: pawn row first not one",
            program=SERVER_BIN,
            args=["-r", "001", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="first and last pawn must be 1"
        ),
        Case(
            name="server: pawn row last not one",
            program=SERVER_BIN,
            args=["-r", "100", "-a", "127.0.0.1", "-p", "1234", "-t", "5"],
            expected_code=1,
            stderr_contains="first and last pawn must be 1"
        ),
        Case(
            name="server: minimal valid pawn row",
            program=SERVER_BIN,
            args=["-r", "1", "-a", "127.0.0.1", "-p", "1234", "-t", "1"],
            expected_behavior="timeout"
        ),
        Case(
            name="server: normal valid row",
            program=SERVER_BIN,
            args=["-r", "11101111011111", "-a", "127.0.0.1", "-p", "1234", "-t", "1"],
            expected_behavior="timeout"
        ),
        Case(
            name="server: last row wins",
            program=SERVER_BIN,
            args=["-r", "001", "-r", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "1"],
            expected_behavior="timeout"
        ),
        Case(
            name="server: last timeout wins",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "127.0.0.1", "-p", "1234", "-t", "200", "-t", "1"],
            expected_behavior="timeout"
        ),
        Case(
            name="server: last address wins",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "bad.invalid", "-a", "127.0.0.1", "-p", "1234", "-t", "1"],
            expected_behavior="timeout"
        ),
        Case(
            name="server: last port wins",
            program=SERVER_BIN,
            args=["-r", "101", "-a", "127.0.0.1", "-p", "0", "-p", "1234", "-t", "1"],
            expected_behavior="timeout"
        ),
    ]


class ParsingTests(unittest.TestCase):
    maxDiff = None

    def test_all_cases(self):
        for case in client_cases() + server_cases():
            with self.subTest(case=case.name):
                ok, message = run_case(case)
                self.assertTrue(ok, msg=f"{case.name}\n{message}")


class ColorTextTestResult(unittest.TextTestResult):
    def addSuccess(self, test):
        super().addSuccess(test)
        print(Fore.GREEN + "✔ PASS:", test)

    def addFailure(self, test, err):
        super().addFailure(test, err)
        print(Fore.RED + "✘ FAIL:", test)

    def addError(self, test, err):
        super().addError(test, err)
        print(Fore.MAGENTA + "💥 ERROR:", test)


class ColorTextTestRunner(unittest.TextTestRunner):
    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            resultclass=ColorTextTestResult,
            verbosity=2,
            **kwargs
        )


if __name__ == "__main__":
    unittest.main(testRunner=ColorTextTestRunner)