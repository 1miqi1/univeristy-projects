import subprocess
import unittest
import time
import os
import signal

SERVER_BIN = './kayles_server'
CLIENT_BIN = './kayles_client'

class KaylesComprehensiveTest(unittest.TestCase):

    def assert_negative(self, cmd):
        """Sprawdza, czy program kończy się kodem 1 i wypisuje błąd na stderr."""
        result = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1, f"Miał być błąd (1), a jest {result.returncode} dla: {' '.join(cmd)}")
        self.assertTrue(len(result.stderr.strip()) > 0, "Brak komunikatu o błędzie na stderr")

    def assert_positive_server(self, cmd):
        """Sprawdza, czy serwer poprawnie parsuje argumenty i zaczyna nasłuchiwać."""
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, preexec_fn=os.setsid)
        try:
            time.sleep(0.2)
            ret = proc.poll()
            if ret is not None:
                err = proc.stderr.read()
                self.assertEqual(ret, 0, f"Serwer zamknął się przedwcześnie z błędem: {err}")
            else:
                # Jeśli działa, to znaczy, że argumenty były OK
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        finally:
            proc.stdout.close()
            proc.stderr.close()
            proc.wait()

    def assert_positive_client(self, cmd):
        """Sprawdza, czy klient poprawnie wysyła wiadomość (kod 0)."""
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, f"Klient winien zwrócić 0, a zwrócił {result.returncode}")

    # === TESTY SERWERA: ARGUMENTY ===

    def test_server_valid_params(self):
        """Testy poprawnych kombinacji parametrów serwera."""
        cases = [
            ['-r', '1', '-a', '127.0.0.1', '-p', '10001', '-t', '1'],
            ['-r', '101', '-a', '0.0.0.0', '-p', '0', '-t', '99'],
            ['-r', '1' * 256, '-a', 'localhost', '-p', '10002', '-t', '5'], # Max długość rzędu
        ]
        for args in cases:
            self.assert_positive_server([SERVER_BIN] + args)

    def test_server_invalid_pawn_row(self):
        """Testy walidacji rzędu pionów (-r)."""
        base = ['-a', '127.0.0.1', '-p', '10003', '-t', '10']
        invalid_rows = ['', '011', '110', '1021', '1A1', '1' * 257]
        for row in invalid_rows:
            self.assert_negative([SERVER_BIN, '-r', row] + base)

    def test_server_invalid_address(self):
        """Testy walidacji adresu (-a)."""
        base = ['-r', '101', '-p', '10004', '-t', '10']
        invalid_addrs = ['999.999.999.999', '127.0.1', 'not-a-domain.invalid-tld']
        for addr in invalid_addrs:
            self.assert_negative([SERVER_BIN, '-a', addr] + base)

    def test_server_invalid_port_and_timeout(self):
        """Testy walidacji portu (-p) i czasu (-t)."""
        self.assert_negative([SERVER_BIN, '-r', '101', '-a', '127.0.0.1', '-p', '65536', '-t', '10'])
        self.assert_negative([SERVER_BIN, '-r', '101', '-a', '127.0.0.1', '-p', '-1', '-t', '10'])
        self.assert_negative([SERVER_BIN, '-r', '101', '-a', '127.0.0.1', '-p', '8080', '-t', '0'])
        self.assert_negative([SERVER_BIN, '-r', '101', '-a', '127.0.0.1', '-p', '8080', '-t', '100'])

    # === TESTY KLIENTA: ARGUMENTY I WIADOMOŚCI ===

    def test_client_valid_messages(self):
        """Testy poprawnych formatów wiadomości klienta."""
        # Nawet bez serwera, klient powinien zwrócić 0 po timeoutcie
        cases = [
            '0/123',            # JOIN
            '1/123/456/0',      # MOVE_1
            '2/123/456/0',      # MOVE_2
            '3/123/456',        # KEEP_ALIVE
            '4/123/456',        # GIVE_UP
        ]
        for msg in cases:
            self.assert_positive_client([CLIENT_BIN, '-a', '127.0.0.1', '-p', '10005', '-t', '1', '-m', msg])

    def test_client_domain_resolution(self):
        """Testy klienta z istniejącymi domenami."""
        # google.com na porcie 80 to typowy test dostępności sieci
        self.assert_positive_client([CLIENT_BIN, '-a', 'google.com', '-p', '80', '-t', '1', '-m', '0/1'])
        self.assert_positive_client([CLIENT_BIN, '-a', 'localhost', '-p', '10006', '-t', '1', '-m', '0/1'])

    def test_client_invalid_message_logic(self):
        """Testy błędnych formatów pola -m."""
        base = ['-a', '127.0.0.1', '-p', '10007', '-t', '1']
        invalid_msgs = [
            '0',                # Za mało pól dla JOIN
            '0/123/456',        # Za dużo pól dla JOIN
            '1/123/456',        # Za mało pól dla MOVE_1
            '2/123/456/0/9',    # Za dużo pól dla MOVE_2
            '5/123/456',        # Nieznany typ 5
            '0/abc',            # Litery zamiast ID
            '1/123/4294967296/0'# GameID > 32 bit
        ]
        for msg in invalid_msgs:
            self.assert_negative([CLIENT_BIN, '-m', msg] + base)

    def test_missing_parameters(self):
        """Sprawdza, czy brak któregokolwiek z obowiązkowych parametrów rzuca błąd."""
        # Serwer
        self.assert_negative([SERVER_BIN, '-a', '127.0.0.1', '-p', '8080', '-t', '10']) # Brak -r
        self.assert_negative([SERVER_BIN, '-r', '101', '-p', '8080', '-t', '10'])      # Brak -a
        # Klient
        self.assert_negative([CLIENT_BIN, '-p', '8080', '-t', '1', '-m', '0/1'])       # Brak -a
        self.assert_negative([CLIENT_BIN, '-a', '127.0.0.1', '-t', '1', '-m', '0/1'])  # Brak -p

if __name__ == '__main__':
    import unittest
    from colorama import Fore, Style, init
    init(autoreset=True)
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
                failfast=False,  # 🔥 stops on first failure
                **kwargs
            )

    unittest.main(testRunner=ColorTextTestRunner)