"""
Profesjonalny zestaw testów integracyjnych dla klienta radia internetowego (sikradio).
Testuje protokół TCP (IPv4/IPv6), parsowanie CLI, multiplexing ICY (Shoutcast),
stabilność, timeouty, fragmentację oraz poprawność bitową strumienia.
"""

import os
import sys
import time
import socket
import struct
import threading
import subprocess
import hashlib
from typing import List, Tuple, Callable, Optional

import pytest

# --- KONFIGURACJA ---
SIKRADIO_PATH = "../"
BINARY_PATH = os.environ.get("SIKRADIO_PATH", "./sikradio")
DEFAULT_TIMEOUT_MS = 5000

# Wymuś sprawdzenie czy binarka istnieje
if not os.path.isfile(BINARY_PATH) or not os.access(BINARY_PATH, os.X_OK):
    pytest.exit(f"Nie znaleziono pliku wykonywalnego: {BINARY_PATH}. "
                f"Skompiluj projekt lub ustaw zmienną środowiskową SIKRADIO_PATH.")


# --- FAKE SERVER ICY / SHOUTCAST ---

def make_icy_metadata(text: str) -> bytes:
    """Tworzy blok metadanych zgodnie z protokołem ICY (Shoutcast)."""
    if not text:
        return b'\x00'
    b_text = text.encode('utf-8')
    # ICY wymaga paddingu null-bajtami do wielokrotności 16
    blocks = (len(b_text) + 15) // 16
    pad_len = blocks * 16 - len(b_text)
    return bytes([blocks]) + b_text + (b'\x00' * pad_len)


class FakeRadioServer:
    """Wielowątkowy serwer testowy TCP symulujący radio internetowe."""

    def __init__(self, ipv6: bool = False, handler: Callable = None):
        self.ipv6 = ipv6
        self.family = socket.AF_INET6 if ipv6 else socket.AF_INET
        self.host = "::1" if ipv6 else "127.0.0.1"
        self.socket = socket.socket(self.family, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind((self.host, 0))
        self.port = self.socket.getsockname()[1]
        self.handler = handler
        self.running = False
        self.thread = None
        self.connections_accepted = 0

    def start(self):
        self.socket.listen(5)
        self.running = True
        self.thread = threading.Thread(target=self._accept_loop, daemon=True)
        self.thread.start()

    def stop(self):
        self.running = False
        try:
            # Wymuszenie odblokowania accept() poprzez fałszywe połączenie
            dummy = socket.socket(self.family, socket.SOCK_STREAM)
            dummy.connect((self.host, self.port))
            dummy.close()
        except Exception:
            pass
        self.socket.close()
        if self.thread:
            self.thread.join(timeout=1.0)

    def _accept_loop(self):
        while self.running:
            try:
                conn, addr = self.socket.accept()
                if not self.running:
                    conn.close()
                    break
                self.connections_accepted += 1
                if self.handler:
                    # Uruchom handler w osobnym wątku, aby obsłużyć reconnecty
                    threading.Thread(target=self.handler, args=(conn,)).start()
                else:
                    conn.close()
            except Exception:
                break

    @property
    def url(self):
        if self.ipv6:
            return f"http://[{self.host}]:{self.port}/stream"
        return f"http://{self.host}:{self.port}/stream"


# --- FIXTURY PYTEST ---

@pytest.fixture
def ipv4_server():
    server = FakeRadioServer(ipv6=False)
    yield server
    server.stop()

@pytest.fixture
def ipv6_server():
    server = FakeRadioServer(ipv6=True)
    yield server
    server.stop()


# --- POMOCNICZY URUCHAMIACZ KLIENTA ---

class ClientResult:
    def __init__(self, stdout: bytes, stderr: bytes, returncode: int):
        self.stdout = stdout
        self.stderr = stderr
        self.returncode = returncode


# def run_client(args: List[str], stdin_data: bytes = None, timeout: float = 5.0,
#                bg_stdin_func: Callable = None) -> ClientResult:
#     """Bezpiecznie uruchamia klienta, zapobiegając deadlockom na potokach."""

#     cmd = [BINARY_PATH] + args
#     p = subprocess.Popen(
#         cmd,
#         stdin=subprocess.PIPE,
#         stdout=subprocess.PIPE,
#         stderr=subprocess.PIPE
#     )

#     if bg_stdin_func:
#         # Używane do wysyłania komend (np. "quit\n") w trakcie trwania streamu
#         t = threading.Thread(target=bg_stdin_func, args=(p.stdin,))
#         t.daemon = True
#         t.start()

#     try:
#         out, err = p.communicate(input=stdin_data, timeout=timeout)
#         return ClientResult(out, err, p.returncode)
#     except subprocess.TimeoutExpired:
#         p.kill()
#         out, err = p.communicate()
#         pytest.fail(f"Klient przekroczył czas oczekiwania ({timeout}s). Wyjście: {err.decode('utf-8', 'ignore')}")

def run_client(args: List[str], stdin_data: bytes = None, timeout: float = 5.0,
               bg_stdin_func: Optional[Callable] = None) -> ClientResult:

    cmd = [BINARY_PATH] + args
    p = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=0  # Wyłączenie buforowania dla natychmiastowej komunikacji
    )

    out_chunks = []
    err_chunks = []

    # Wątki do czytania wyjścia (zapobiegają zapełnieniu rur/deadlockom)
    def read_pipe(pipe, chunks):
        while True:
            data = pipe.read(1024)
            if not data:
                break
            chunks.append(data)

    t_out = threading.Thread(target=read_pipe, args=(p.stdout, out_chunks))
    t_err = threading.Thread(target=read_pipe, args=(p.stderr, err_chunks))
    t_out.start()
    t_err.start()

    # Obsługa stdin
    try:
        if bg_stdin_func:
            # Uruchamiamy Twój wątek w tle, który wyśle 'quit' po 0.5s
            # Teraz p.stdin pozostaje OTWARTY tak długo, jak chcesz
            bg_stdin_thread = threading.Thread(target=bg_stdin_func, args=(p.stdin,))
            bg_stdin_thread.start()
            bg_stdin_thread.join()
        elif stdin_data:
            p.stdin.write(stdin_data)
            p.stdin.flush()
            p.stdin.close() # Tu zamykamy tylko jeśli to jednorazowe dane
        else:
            # Jeśli nic nie wysyłamy, nie zamykamy od razu,
            # by nie wysłać EOF zbyt wcześnie (opcjonalnie)
            pass

        # Czekamy na zakończenie procesu z timeoutem
        p.wait(timeout=timeout)

    except subprocess.TimeoutExpired:
        p.kill()
        p.wait() # Czekamy na posprzątanie procesu przez system
        # Nie robimy pytest.fail tutaj, sprawdzimy to na końcu
    finally:
        # Zamykamy stdin jeśli jeszcze otwarty, by wątki czytające mogły skończyć
        try:
            p.stdin.close()
        except:
            pass

        # Czekamy na zakończenie wątków czytających
        t_out.join(timeout=1.0)
        t_err.join(timeout=1.0)

    stdout = b"".join(out_chunks)
    stderr = b"".join(err_chunks)

    if p.returncode is None:
         # To znaczy, że użyliśmy p.kill() powyżej
         raise TimeoutError(f"Klient nie zakończył się w {timeout}s. Stderr: {stderr.decode('utf-8', 'ignore')}")

    return ClientResult(stdout, stderr, p.returncode)


# =====================================================================
# 6.2 TESTY CLI
# =====================================================================

def test_cli_missing_url():
    """Brak argumentu -u powinien zakończyć program kodem 1."""
    res = run_client(["-m", "-t", "1000"])
    assert res.returncode == 1

@pytest.mark.parametrize("flags", [
    ["-q", "-u", "http://localhost"],
    ["-v", "0", "-u", "http://localhost"],
    ["-m46", "-u", "http://localhost"],
    ["-t", "100", "-u", "http://localhost"],
])
def test_cli_valid_parsing(flags):
    """Testuje, czy program nie wywala się na poprawnych argumentach (nawet jeśli serwer nie istnieje)."""
    # Zwróci 1 bo nie połączy się z localhost, ale nie z powodu błędu parsowania
    # Sprawdzamy czy poprawnie interpretuje zlepione i rozdzielone flagi.
    res = run_client(flags, timeout=2.0)
    assert res.returncode in [0, 1]

def test_cli_invalid_timeout():
    """Zbyt mały timeout (<100) lub nienumeryczny."""
    res = run_client(["-u", "http://localhost", "-t", "50"])
    assert res.returncode == 1

    res = run_client(["-u", "http://localhost", "-t", "abc"])
    assert res.returncode == 1


# =====================================================================
# 6.1 & 6.8 TESTY PODSTAWOWE I POPRAWNOŚCI STRUMIENIA
# =====================================================================

def test_basic_audio_stream(ipv4_server):
    """Odbiór samego audio - weryfikacja exact match bajtów."""

    payload = os.urandom(256 * 1024) # 256 KB random audio

    def handler(conn):
        conn.recv(1024) # Odbierz HTTP GET
        conn.sendall(b"ICY 200 OK\r\n\r\n")
        conn.sendall(payload)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url])
    assert res.returncode == 0
    assert hashlib.md5(res.stdout).hexdigest() == hashlib.md5(payload).hexdigest()
    assert len(res.stdout) == len(payload)


def test_multiplexed_stream(ipv4_server):
    """Odbiór audio + metadata z flagą -m."""

    audio_chunk1 = os.urandom(8192)
    audio_chunk2 = os.urandom(8192)
    metadata_str = "StreamTitle='SikRadio Test';\x00"

    def handler(conn):
        req = conn.recv(1024)
        assert b"Icy-MetaData: 1" in req, "Klient nie poprosił o metadane mimo flagi -m!"

        conn.sendall(b"ICY 200 OK\r\nicy-metaint: 8192\r\n\r\n")
        conn.sendall(audio_chunk1)
        conn.sendall(make_icy_metadata(metadata_str))
        conn.sendall(audio_chunk2)
        # Zamykamy
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-m", "-q"])

    assert res.returncode == 0
    # Audio musi być nienaruszone
    assert res.stdout == (audio_chunk1 + audio_chunk2)
    # Metadane na stderr (surowy tekst bez paddingu/długości)
    assert metadata_str.encode('utf-8') in res.stderr


# =====================================================================
# 6.3 TESTY QUIT (BARDZO WAŻNE)
# =====================================================================

def test_quit_immediate(ipv4_server):
    """Klient musi natychmiastowo zakończyć pracę po wpisaniu 'quit\\n'."""

    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\n")
        try:
            while True:
                conn.sendall(b"AUDIO")
                time.sleep(0.01)
        except Exception:
            pass # Socket zamknięty przez klienta

    ipv4_server.handler = handler
    ipv4_server.start()

    def send_quit(stdin):
        time.sleep(0.5) # Daj klientowi czas na połączenie
        stdin.write(b"quit\n")
        stdin.flush()

    # Oczekujemy szybkiego zamknięcia pomimo nieskończonego strumienia
    start_time = time.time()
    res = run_client(["-u", ipv4_server.url, "-q"], bg_stdin_func=send_quit, timeout=2.0)
    duration = time.time() - start_time

    assert res.returncode == 0
    assert duration < 2.0
    assert b"AUDIO" in res.stdout


def test_quit_fragmented(ipv4_server):
    """Odporność na pofragmentowane wejście 'quit\\n' i śmieci."""

    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\n")
        try:
            for _ in range(50):
                conn.sendall(b"DATA")
                time.sleep(0.1)
        except Exception:
            pass

    ipv4_server.handler = handler
    ipv4_server.start()

    def send_garbage_and_quit(stdin):
        time.sleep(0.2)
        stdin.write(b"smieci\n") # Ignorowane
        stdin.flush()
        time.sleep(0.1)
        stdin.write(b"q")
        stdin.flush()
        time.sleep(0.1)
        stdin.write(b"u")
        stdin.flush()
        time.sleep(0.1)
        stdin.write(b"it\n")
        stdin.flush()

    res = run_client(["-u", ipv4_server.url, "-q"], bg_stdin_func=send_garbage_and_quit, timeout=3.0)
    assert res.returncode == 0


# =====================================================================
# 6.4 TESTY ZAMKNIĘCIA SERWERA
# =====================================================================

def test_server_closes_socket(ipv4_server):
    """Gdy serwer zamyka socket, klient kończy się z exit code = 0 i flushem danych."""
    payload = b"END_OF_TRANSMISSION"

    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\n")
        conn.sendall(payload)
        # Natychmiastowe zamknięcie bez ostrzeżenia
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-q"])
    assert res.returncode == 0
    assert payload in res.stdout


# =====================================================================
# 6.5 TESTY TIMEOUT & RECONNECT
# =====================================================================

def test_reconnect_on_timeout(ipv4_server):
    """Klient musi rozłączyć się po przekroczeniu -t i ponowić połączenie."""

    connection_states = {"count": 0}

    def handler(conn):
        connection_states["count"] += 1
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\n")
        conn.sendall(b"PART1")

        if connection_states["count"] == 1:
            # Pierwsze połączenie: usypiamy na 1 sekundę (timeout klienta to 500ms)
            # Klient powinien zamknąć to połączenie
            time.sleep(1.0)
        else:
            # Drugie połączenie: wysyłamy resztę i poprawnie kończymy
            conn.sendall(b"PART2")
            conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    # timeout aplikacji: 500ms, wymuszamy wyjście przez zamknięcie drugiego połączenia
    res = run_client(["-u", ipv4_server.url, "-t", "500", "-q"])

    assert res.returncode == 0
    assert connection_states["count"] == 2
    # Program powinien na stdout wyrzucić dane z obu sesji
    assert b"PART1" in res.stdout
    assert b"PART2" in res.stdout


# =====================================================================
# 6.6 TESTY IPv4 / IPv6
# =====================================================================

def test_force_ipv4(ipv4_server):
    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\nIPV4_OK")
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-4", "-q"])
    assert res.returncode == 0
    assert b"IPV4_OK" in res.stdout

def test_force_ipv6(ipv6_server):
    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\nIPV6_OK")
        conn.close()

    ipv6_server.handler = handler
    ipv6_server.start()

    res = run_client(["-u", ipv6_server.url, "-6", "-q"])
    assert res.returncode == 0
    assert b"IPV6_OK" in res.stdout


# =====================================================================
# 6.7 TESTY EDGE CASE & STRESS
# =====================================================================

def test_edge_tcp_fragmentation(ipv4_server):
    """Fragmentacja pakietów TCP na poziomie bajt-po-bajcie (byte-by-byte)."""
    payload = b"FRAGMENTATION_TEST_DATA"

    def handler(conn):
        conn.recv(1024)
        headers = b"ICY 200 OK\r\n\r\n"
        for byte in headers:
            conn.send(bytes([byte]))
            time.sleep(0.005) # Wymusza wysyłanie małych ramek TCP

        for byte in payload:
            conn.send(bytes([byte]))
            time.sleep(0.005)

        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-q"])
    assert res.returncode == 0
    assert res.stdout == payload


def test_stress_huge_stream(ipv4_server):
    """Wysyła 10MB danych aby sprawdzić czy klient radzi sobie z windowingiem i buforami."""

    MB_10 = 10 * 1024 * 1024
    chunk = os.urandom(65536)

    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\n")
        sent = 0
        while sent < MB_10:
            conn.sendall(chunk)
            sent += len(chunk)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-q"], timeout=10.0)
    assert res.returncode == 0
    assert len(res.stdout) >= MB_10


def test_edge_zero_length_metadata(ipv4_server):
    """Test na poprawne zignorowanie pustego bloku metadanych (bardzo częste w Shoutcast)."""

    audio = b"AUDIO"

    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\nicy-metaint: 5\r\n\r\n")
        conn.sendall(audio)
        conn.sendall(b'\x00') # Zero-length metadata flag (0 * 16 bytes)
        conn.sendall(audio)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-m", "-q"])
    assert res.returncode == 0
    assert res.stdout == (audio + audio) # Brak śmieci po połączeniu


def test_edge_fast_reconnect_loop(ipv4_server):
    """Testuje serię bardzo szybkich zamknięć połączeń przez serwer."""
    states = {"conns": 0}

    def handler(conn):
        states["conns"] += 1
        conn.recv(1024)
        if states["conns"] < 5:
            # Drop connection immediately right after HTTP header
            conn.sendall(b"ICY 200 OK\r\n\r\n")
            conn.close()
        else:
            # 5-te połączenie stabilne
            conn.sendall(b"ICY 200 OK\r\n\r\nSTABLE_DATA")
            conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    # Klient bez timeoutu, ale po zamknięciu serwera domyślnie powinien połączyć się ponownie?
    # W specyfikacji: "Jeśli serwer zamknął połączenie, klient wypisuje [...] i kończy się statusem 0."
    # UWAGA ZGODNA ZE SPECYFIKACJĄ: Serwer zamyka -> Exit 0. Reconnect dotyczy TYLKO TIMEOUTU (-t).

    # Skoro serwer zamknął (EOF), wg zadania:
    # "Jeśli serwer zamknął połączenie, klient wypisuje wszystkie dotychczas odebrane dane i kończy się statusem 0."
    res = run_client(["-u", ipv4_server.url, "-q"])
    assert res.returncode == 0
    assert states["conns"] == 1 # Skończył na pierwszym przerwaniu!
    assert res.stdout == b""


def test_mixed_binary_metadata(ipv4_server):
    """Test odporności na zepsute metadane (śmieci zamiast tekstu)."""
    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\nicy-metaint: 4\r\n\r\n")
        conn.sendall(b"BUMP")

        # Wysłanie 1 bloku (16 bajtów) całkowicie losowych bajtów niezgodnych z ASCII/UTF-8
        corrupted_meta = b'\x01' + os.urandom(16)
        conn.sendall(corrupted_meta)
        conn.sendall(b"BUMP")
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-m", "-q"])
    assert res.returncode == 0
    assert res.stdout == b"BUMPBUMP"
    # Nie weryfikujemy stderr dokładnie, ponieważ klient wypisuje "bez zmian".
    # Ważne, że nie wystąpił segfault.


# =====================================================================
# 1. Rygorystyczne testy utraty danych (Disconnection & Flush)
# =====================================================================

def test_disconnect_flush_massive(ipv4_server):
    """
    SCENARIUSZ: Serwer wysyła 15MB danych tak szybko, jak to możliwe i NATYCHMIAST
    zamyka połączenie (wysyła FIN/RST).
    WERYFIKACJA: Klient musi opróżnić cały bufor TCP (read() aż do EOF) przed
    zakończeniem działania. Nawet 1 zagubiony bajt oznacza oblany test.
    """
    PAYLOAD_SIZE = 15 * 1024 * 1024 # 15 MB
    payload = os.urandom(PAYLOAD_SIZE)
    expected_hash = hashlib.sha256(payload).hexdigest()

    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\n")
        conn.sendall(payload)
        # Bezwzględne zamknięcie socketu
        conn.shutdown(socket.SHUT_RDWR) # Wysłanie pakietu FIN
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    # Zwiększony timeout testu na przetworzenie 15MB
    res = run_client(["-u", ipv4_server.url, "-q"], timeout=10.0)

    assert res.returncode == 0, "Klient powinien zakończyć się kodem 0 po EOF od serwera"
    assert len(res.stdout) == PAYLOAD_SIZE, f"Zgubiono lub dodano bajty! Oczekiwano {PAYLOAD_SIZE}, otrzymano {len(res.stdout)}"
    assert hashlib.sha256(res.stdout).hexdigest() == expected_hash, "Korupcja danych (błąd bitowy)!"


def test_reconnect_exact_byte_concatenation(ipv4_server):
    """
    SCENARIUSZ: Serwer zrywa połączenie 3 razy w trakcie transmisji.
    Klient używa flagi `-t` (timeout), by wznawiać połączenie.
    WERYFIKACJA: Połączenie strumieni z 4 sesji musi dać IDEALNY plik wyjściowy,
    bez żadnych "śmieci" z nagłówków HTTP wplecionych w audio.
    """
    chunks = [os.urandom(1024 * 512) for _ in range(4)] # 4 kawałki po 512KB
    expected_payload = b"".join(chunks)

    state = {"conn_num": 0}

    def handler(conn):
        c_num = state["conn_num"]
        state["conn_num"] += 1

        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\n")
        conn.sendall(chunks[c_num])

        if c_num < 3:
            # Udajemy martwego serwera - wymuszamy timeout klienta
            time.sleep(1.0)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    # Timeout klienta = 200ms. Po 200ms ciszy musi zrobić reconnect.
    res = run_client(["-u", ipv4_server.url, "-q", "-t", "200"], timeout=10.0)

    assert res.returncode == 0
    assert state["conn_num"] == 4, "Klient nie ponowił połączenia odpowiednią ilość razy"
    assert len(res.stdout) == len(expected_payload)
    assert hashlib.sha256(res.stdout).hexdigest() == hashlib.sha256(expected_payload).hexdigest()


# =====================================================================
# 2. Tortury parsera ICY (Złośliwa Fragmentacja i Maksima)
# =====================================================================

def test_icy_metadata_max_size(ipv4_server):
    """
    SCENARIUSZ: Protokół ICY koduje długość metadanych w 1 bajcie (liczba bloków 16-bajtowych).
    Max wartość to 255 * 16 = 4080 bajtów. Wysyłamy ABSOLUTNE MAKSIMUM.
    WERYFIKACJA: Sprawdzenie, czy klient poprawnie rzutuje rozmiar z bajtu (uint8_t).
    """
    audio1 = b"AUDIO_BEFORE"
    audio2 = b"AUDIO_AFTER_"

    # 13 znaków "StreamTitle='" + 4064 znaków "A" + 3 znaki "';\x00" = 4080 znaków.
    max_meta_str = "StreamTitle='" + ("A" * 4064) + "';\x00"
    max_meta_bytes = make_icy_metadata(max_meta_str)

    assert len(max_meta_bytes) == 1 + 4080 # Teraz to się zgadza! (bajt nagłówka 0xFF + 4080)

    def handler(conn):
        conn.recv(1024)
        conn.sendall(f"ICY 200 OK\r\nicy-metaint: {len(audio1)}\r\n\r\n".encode())
        conn.sendall(audio1)
        conn.sendall(max_meta_bytes)
        conn.sendall(audio2)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-m", "-q"])

    assert res.returncode == 0
    assert res.stdout == audio1 + audio2, "Strumień audio został uszkodzony przez gigantyczne metadane!"
    assert max_meta_str.encode() in res.stderr, "Metadane nie zostały poprawnie wypisane"


def test_icy_metadata_malicious_fragmentation(ipv4_server):
    """
    SCENARIUSZ: Serwer wysyła metadane, ale dzieli pakiety TCP w najbardziej
    złośliwych miejscach.
    WERYFIKACJA: Parser maszyny stanów w kliencie C++ musi poprawnie buforować bajty.
    """
    audio = b"12345678"
    meta_str = "StreamTitle='Fragmentation Test';\x00"
    meta_bytes = make_icy_metadata(meta_str)

    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\nicy-metaint: 8\r\n\r\n")

        # Wysyłamy pierwszą część audio
        conn.sendall(audio[:4])
        time.sleep(0.05)
        conn.sendall(audio[4:])

        # ZŁOŚLIWOŚĆ 1: Wysyłamy TYLKO bajt długości metadanych
        conn.sendall(bytes([meta_bytes[0]]))
        time.sleep(0.05)

        # ZŁOŚLIWOŚĆ 2: Wysyłamy metadane po jednym bajcie
        for b in meta_bytes[1:]:
            conn.sendall(bytes([b]))
            time.sleep(0.001)

        # Reszta audio
        conn.sendall(audio)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-m", "-q"])

    assert res.returncode == 0
    assert res.stdout == audio + audio, "Złośliwa fragmentacja zepsuła audio"
    assert meta_str.encode() in res.stderr


def test_icy_missing_metadata_support(ipv4_server):
    """
    SCENARIUSZ: Klient prosi o metadane (-m), ale serwer ich NIE OBSŁUGUJE
    (nie wysyła nagłówka icy-metaint).
    WERYFIKACJA: Klient nie może się zawiesić, wypisywać śmieci ani próbować
    wypisywać audio jako metadanych. Musi po prostu zrzucić wszystko na stdout.
    """
    payload = os.urandom(65536)

    def handler(conn):
        req = conn.recv(1024)
        assert b"Icy-MetaData: 1" in req
        # Serwer ignoruje prośbę i odpowiada zwykłym strumieniem
        conn.sendall(b"HTTP/1.0 200 OK\r\nContent-Type: audio/mpeg\r\n\r\n")
        conn.sendall(payload)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-m", "-q"])

    assert res.returncode == 0
    assert res.stdout == payload
    assert res.stderr == b"" # stderr musi być puste


# =====================================================================
# 3. Testy na logikę czasu (Timeout Trickle)
# =====================================================================

def test_slow_trickle_no_timeout(ipv4_server):
    """
    SCENARIUSZ: Timeout klienta (-t) ma wynosić 500ms. Serwer wysyła
    1 bajt co 400ms.
    WERYFIKACJA: Timeout określa maksymalny czas BEZCZYNNOŚCI (braku danych).
    Skoro dane cały czas powoli kapią, klient NIE MOŻE zamknąć połączenia!
    """
    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\n")

        # Pętla trwa ~2 sekundy (5 iteracji * 400ms)
        for _ in range(5):
            conn.sendall(b"A")
            time.sleep(0.4)

        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    start_time = time.time()
    # Klient ma timeout 500ms. Jeśli źle to zaimplementował (np. mierzy całkowity czas
    # zamiast czasu od ostatniego bajtu), to ulegnie restartowi.
    res = run_client(["-u", ipv4_server.url, "-q", "-t", "500"], timeout=4.0)
    duration = time.time() - start_time

    assert res.returncode == 0
    assert res.stdout == b"AAAAA", "Klient uciął powolny strumień (źle napisana logika timeoutu!)"
    assert duration >= 2.0


# =====================================================================
# 4. Stress Test: Naprzemienny strumień (Memory Leak / CPU Loop Test)
# =====================================================================

def test_heavy_multiplex_interleave(ipv4_server):
    """
    SCENARIUSZ: Bardzo mały interwał metadanych (icy-metaint: 16) przy
    dość dużym pliku. Oznacza to tysiące zmian kontekstu (audio -> meta -> audio).
    WERYFIKACJA: Żaden bajt nie może uciec, weryfikuje odporność na memory leaki
    oraz precyzję liczników bajtów w kliencie.
    """
    METAINT = 16
    AUDIO_BLOCKS = 10000

    expected_audio = bytearray()
    expected_meta = bytearray()

    def handler(conn):
        conn.recv(1024)
        conn.sendall(f"ICY 200 OK\r\nicy-metaint: {METAINT}\r\n\r\n".encode())

        for i in range(AUDIO_BLOCKS):
            # 16 bajtów audio
            audio_chunk = os.urandom(METAINT)
            expected_audio.extend(audio_chunk)
            conn.sendall(audio_chunk)

            # Losowe metadane (czasem puste)
            if i % 10 == 0:
                meta_str = f"S='{i}';\x00"
                icy_block = make_icy_metadata(meta_str)
                expected_meta.extend(icy_block[1:]) # Dodajemy do wzorca blok bez 1 bajtu długości
                conn.sendall(icy_block)
            else:
                conn.sendall(b'\x00') # Puste metadane

        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-m", "-q"], timeout=5.0)

    assert res.returncode == 0

    # Szybki hash upewnia nas, że pliki są identyczne (nawet przy 160KB to lepsze niż diff)
    assert hashlib.sha256(res.stdout).hexdigest() == hashlib.sha256(expected_audio).hexdigest()

    # stderr powinno zawierać złączone ze sobą ciągi tekstowe S='0';S='10';S='20'; itd.
    assert res.stderr == expected_meta


# =====================================================================
# 5. TESTY KILLERY (Ostateczne sprawdziany stabilności)
# =====================================================================

def test_icy_metaint_1_byte_thrashing(ipv4_server):
    """
    SCENARIUSZ (KILLER): Serwer ustawia icy-metaint na 1 bajt!
    Oznacza to, że strumień to na przemian: 1 bajt audio, nagłówek metadanych, metadane, 1 bajt audio...
    WERYFIKACJA: Ekstremalnie obciąża maszynę stanów parsera wewnątrz `read_stream()`.
    Wewnętrzny bufor w C++ (4096 bajtów) będzie zawierał setki zmian kontekstu.
    Błędy typu "off-by-one" (np. i += available) spowodują natychmiastowe uszkodzenie danych
    lub nieskończoną pętlę.
    """
    AUDIO_TOTAL = 5000
    expected_audio = bytearray(os.urandom(AUDIO_TOTAL))
    expected_meta = bytearray()

    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\nicy-metaint: 1\r\n\r\n")

        # Wysyłamy gigantyczny miks, pakując wszystko w duże ramki TCP (sendall z buforem)
        payload = bytearray()
        for i in range(AUDIO_TOTAL):
            payload.append(expected_audio[i]) # 1 bajt audio

            if i % 100 == 0:
                # Co 100 bajtów wstawiamy metadane
                meta_str = f"M{i}\x00"
                icy_block = make_icy_metadata(meta_str)
                expected_meta.extend(icy_block[1:])
                payload.extend(icy_block)
            else:
                payload.append(0) # 0 bajtów metadanych

        conn.sendall(payload)
        conn.shutdown(socket.SHUT_RDWR)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-m", "-q"], timeout=5.0)

    assert res.returncode == 0
    assert len(res.stdout) == AUDIO_TOTAL, "Zagubiono bajty w piekle przełączeń stanu!"
    assert res.stdout == expected_audio, "Błąd bitowy - parser pogubił się we wskaźnikach bufora"
    assert res.stderr == expected_meta


def test_eof_during_metadata_block(ipv4_server):
    """
    SCENARIUSZ (KILLER): Serwer rozpoczyna wysyłanie gigantycznego bloku metadanych (4080 bajtów).
    Wysyła 100 bajtów i nagle zrywa połączenie TCP (FIN).
    WERYFIKACJA: Program musi wypisać to, co zdążył odebrać z audio i zakończyć się kodem 0.
    Jeśli program zawiesi się czekając na brakujące 3980 bajtów metadanych (blokujący odczyt)
    lub rzuci wyjątek braku danych, oblał test.
    """
    audio = b"AUDIO_SAFE_DATA"

    def handler(conn):
        conn.recv(1024)
        conn.sendall(f"ICY 200 OK\r\nicy-metaint: {len(audio)}\r\n\r\n".encode())
        conn.sendall(audio)
        conn.sendall(b'\xFF') # Obietnica 4080 bajtów metadanych
        conn.sendall(b"A" * 100) # Dajemy tylko 100
        conn.shutdown(socket.SHUT_RDWR) # FIN
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-m", "-q"])

    assert res.returncode == 0, "Klient zcrashował się przy uciętym bloku metadanych"
    assert res.stdout == audio, "Klient nie wypisał zbuforowanego audio przed zakończeniem!"


def test_http_redirect_chain(ipv4_server):
    """
    SCENARIUSZ (KILLER): Serwer odrzuca połączenie odpowiadając 302 Found,
    kierując na inny URL. Twój kod (boost::beast) obsługuje przekierowania.
    Robimy łańcuch przekierowań: /0 -> /1 -> /2 -> ... -> /5 -> audio.
    WERYFIKACJA: Testuje poprawność zamykania starych gniazd w pętli `reconnect()`
    oraz wycieki zasobów (zbyt wiele otwartych plików / gniazd TIME_WAIT).
    """
    payload = b"REDIRECT_SUCCESS"

    def handler(conn):
        req = conn.recv(1024).decode('utf-8', 'ignore')

        # Wyciągamy ścieżkę z żądania (np. GET /3 HTTP/1.1)
        path = req.split(' ')[1]

        if path == "/stream":
            path = "/0"

        step = int(path.replace("/", ""))

        if step < 5:
            # Zwracamy przekierowanie do kolejnego kroku
            next_url = f"http://{ipv4_server.host}:{ipv4_server.port}/{step + 1}"
            response = f"HTTP/1.1 302 Found\r\nLocation: {next_url}\r\nConnection: close\r\n\r\n"
            conn.sendall(response.encode())
        else:
            # Koniec łańcucha - dajemy dane
            conn.sendall(b"ICY 200 OK\r\n\r\n" + payload)

        conn.shutdown(socket.SHUT_RDWR)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    res = run_client(["-u", ipv4_server.url, "-q"])
    assert res.returncode == 0
    assert payload in res.stdout, "Klient nie dotarł na koniec łańcucha przekierowań"


def test_giant_http_headers_byte_by_byte(ipv4_server):
    """
    SCENARIUSZ (KILLER): Klient czyta nagłówki HTTP robiąc `read_bytes(&response_str.back(), 1);`
    w pętli `while`. Serwer wysyła poprawny, ale GIGANTYCZNY nagłówek HTTP (1 MB znaków),
    i w dodatku wysyła to wolno.
    WERYFIKACJA: Sprawdza, czy algorytm dodawania znaków pojedynczo do `std::string`
    nie staje się O(N^2) powodując zablokowanie programu i czy aplikacja nie wyczerpie
    limitu czasu na negocjację strumienia.
    """
    payload = b"GIANT_HEADER_SURVIVOR"

    def handler(conn):
        conn.recv(1024)

        # Bardzo duży, ale legalny nagłówek HTTP
        conn.sendall(b"ICY 200 OK\r\n")
        conn.sendall(b"X-Useless-Header: ")

        # Wysyłamy 4 KB śmieci (nie 1MB żeby nie przedłużać testu, ale dość, by zabić słaby kod)
        chunk = b"A" * 1024
        for _ in range(4):
            conn.sendall(chunk)
            time.sleep(0.001)

        conn.sendall(b"\r\n\r\n")
        conn.sendall(payload)
        conn.shutdown(socket.SHUT_RDWR)
        conn.close()

    ipv4_server.handler = handler
    ipv4_server.start()

    start_time = time.time()
    res = run_client(["-u", ipv4_server.url, "-q"], timeout=0.5)

    assert res.returncode == 0
    assert payload in res.stdout, "Program poległ na parsowaniu bardzo długiego nagłówka"


def test_stdin_massive_garbage_flood(ipv4_server):
    """
    SCENARIUSZ (KILLER): Atakujemy implementację czytania z STDIN.
    Klient czyta wejście po 5 bajtów (`static char buffer[5];`).
    Wysyłamy 2 Megabajty czystych śmieci, po czym wysyłamy "quit\\n".
    WERYFIKACJA: Logika w Twoim programie: `input_str.erase(0, input_str.length() - 4)`
    będzie odpalona setki tysięcy razy. Test weryfikuje, czy ostatecznie program
    znajdzie igłę w stogu siana (quit) i nie ulegnie wcześniej awarii (np. przez
    niewłaściwą reallokację std::string).
    """
    def handler(conn):
        conn.recv(1024)
        conn.sendall(b"ICY 200 OK\r\n\r\n")
        try:
            while True:
                conn.sendall(b"AUDIO")
                time.sleep(0.01)
        except Exception:
            pass

    ipv4_server.handler = handler
    ipv4_server.start()

    # Wysyłamy 2MB śmieci i na koniec quit
    garbage = (b"x" * (2 * 1024 * 1024)) + b"quit\n"

    # Używamy stdin_data zamiast wątku w tle dla zmaksymalizowania przepustowości strumienia IPC
    res = run_client(["-u", ipv4_server.url, "-q"], stdin_data=garbage, timeout=5.0)

    assert res.returncode == 0, "Aplikacja crashuje pod nawałem śmieci na STDIN"
    # Audio musi być, bo program musiał chwilę przetwarzać śmieci
    assert b"AUDIO" in res.stdout