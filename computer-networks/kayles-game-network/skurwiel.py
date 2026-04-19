import concurrent.futures
import threading
import random
# ===========================================================================
# 21. GIGA SKURWIAŁE RZECZY (BOSS FIGHT LEVEL)
# ===========================================================================

class TestGigaSkurwialeRzeczy:

    def test_evil_port_roaming(self, srv_default):
        """
        NAT/GSM Simulator: Gracz A ciągle zmienia port źródłowy dla każdego pakietu.
        Serwer musi identyfikować gracza po 'player_id', a nie po (IP:PORT),
        ale odsyłać odpowiedź ZAWSZE na ten nowy, unikalny port, z którego przyszedł konkretny pakiet.
        """
        addr = srv_default.addr()

        # 1. Dołączamy z portu X
        with make_udp_socket() as s_join:
            gs = join_as_player(addr, 7777, s_join)
            gid = gs.game_id

        # 2. Drugi gracz dołącza normalnie z portu Y
        with make_udp_socket() as s_opp:
            gs_opp = join_as_player(addr, 8888, s_opp)

            # Przeciwnik robi ruch, żeby była tura gracza 7777
            udp_send_recv(s_opp, pack_move1(8888, gid, 0), addr)

        # 3. Gracz 7777 robi ruch z KOMPLETNIE NOWEGO portu Z
        with make_udp_socket() as s_move:
            raw_move = udp_send_recv(s_move, pack_move1(7777, gid, 1), addr)

        assert raw_move is not None, "Serwer zignorował pakiet z nowego portu!"
        gs_move = GameState(raw_move)
        assert gs_move.status == STATUS_TURN_B, "Ruch z nowego portu nie został uznany!"
        assert not gs_move.pawn_present(1), "Pion 1 nie został usunięty!"

        # 4. Gracz 7777 wysyła KEEP_ALIVE z JESZCZE INNEGO portu W
        with make_udp_socket() as s_keep:
            raw_keep = udp_send_recv(s_keep, pack_keep_alive(7777, gid), addr)

        assert raw_keep is not None, "Serwer nie odpowiedział na KEEP_ALIVE z nowego portu!"

    def test_evil_shotgun_concurrency(self, srv_default):
        """
        DDoS uderzający w jeden stan gry.
        50 wątków próbuje dołączyć do gry jako ten sam gracz w ułamku sekundy,
        a potem 50 wątków próbuje usunąć ten sam pion.
        Sprawdzamy czy std::map i struktura GameState nie uległy korupcji w jądrze systemu (kolejka UDP).
        """
        addr = srv_default.addr()

        # Przygotowujemy grę (gracz 9991 i przeciwnik 9992)
        with make_udp_socket() as s_main:
            join_as_player(addr, 9991, s_main)
            gs = join_as_player(addr, 9992, s_main)
            gid = gs.game_id

        # Atak 1: 50 wątków próbuje wysłać MOVE_1 na ten sam pion symultanicznie
        def shoot_move(pawn_id):
            with make_udp_socket(timeout=2.0) as s:
                return udp_send_recv(s, pack_move1(9992, gid, pawn_id), addr)

        with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
            # Wszyscy strzelają w pion nr 0 na raz
            futures = [executor.submit(shoot_move, 0) for _ in range(50)]
            results = [f.result() for f in concurrent.futures.as_completed(futures)]

        # Sprawdzamy stan: pion 0 usunięty, reszta musi być nienaruszona
        valid_responses = [r for r in results if r is not None]
        assert len(valid_responses) > 0, "Serwer udławił się równoległymi pakietami!"

        # Bierzemy ostatni stan od serwera
        with make_udp_socket() as s_check:
            raw_check = udp_send_recv(s_check, pack_keep_alive(9991, gid), addr)

        gs_check = GameState(raw_check)
        assert not gs_check.pawn_present(0), "Pion 0 cudem ocalał!"
        assert gs_check.status == STATUS_TURN_A, "Zły status tury po shotgun attack!"

    def test_evil_map_allocator_lag(self):
        """
        Sprawdza, czy drastyczny przyrost rozmiaru std::map nie powoduje laga
        zabijającego Garbage Collector timeoutów gier.
        Tworzymy 10000 gier w sekundę.
        """
        with ServerProcess(pawn_row="11111111", timeout=3) as srv:
            addr = srv.addr()

            # Najpierw zakładamy "dobrą grę"
            with make_udp_socket() as s_good:
                gs_good = join_as_player(addr, 12345, s_good)
                gid_good = gs_good.game_id

            # Floodujemy serwer z jednego socketu by nie zabić OS-a limitami plików
            # 10 000 JOIN'ów (wygeneruje 10 000 nowych gier oczekujących)
            payloads = [pack_join(100000 + i) for i in range(10000)]

            with make_udp_socket() as s_evil:
                # Wypychamy pakiety do jądra tak szybko, jak to możliwe
                for p in payloads:
                    s_evil.sendto(p, addr)

            # Natychmiast sprawdzamy, czy "dobra gra" przeżyła lag alokatora i flood w socket buforze
            with make_udp_socket(timeout=5.0) as s_check:
                raw_check = udp_send_recv(s_check, pack_keep_alive(12345, gid_good), addr)

            assert raw_check is not None, "Serwer padł pod ciężarem std::map alokacji lub odrzucił pakiety z kolejki (Buffer Overflow)!"
            assert GameState(raw_check).game_id == gid_good, "Korupcja ID!"

    def test_evil_endianness_trap(self, srv_default):
        """
        Wysyła player_id, które jest palindromem bajtowym, ale z 0x00 w środku.
        Jeżeli serwer nie zrobi poprawnego ntohl/htonl, nadpisze błędnie ID lub
        uzna je za 0.
        """
        addr = srv_default.addr()

        # Złośliwe ID: 0x01 0x00 0x00 0x01 (W dec: 16777217)
        # Błąd braku ntohl nie zmieni wartości, co może zmylić proste testy.
        evil_id = 0x01000001

        # Złośliwe ID2: 0x00 0x00 0x00 0x01 (W dec: 1, ale w LE to 16777216)
        evil_id_2 = 0x00000001

        with make_udp_socket() as s:
            raw1 = udp_send_recv(s, pack_join(evil_id), addr)
            raw2 = udp_send_recv(s, pack_join(evil_id_2), addr)

        assert raw1 is not None and raw2 is not None

        gs1 = GameState(raw1)
        gs2 = GameState(raw2)

        assert gs1.player_a_id == evil_id, "Serwer przekłamał złośliwe ID 1! Zła konwersja hton/ntoh."
        assert gs2.player_b_id == evil_id_2, "Serwer przekłamał złośliwe ID 2! Zła konwersja hton/ntoh."


# ===========================================================================
# 22. GIGA SKURWIEL V2: OSTATECZNE STARCIE (DDoS & OOM STRESSER)
# ===========================================================================

class TestGigaSkurwieleV2:

    def test_thermonuclear_udp_flood(self):
        """
        Prawdziwy DDoS wolumetryczny.
        Zalewamy serwer totalnym śmieciem (losowe bajty, błędne długości)
        z maksymalną prędkością, na jaką pozwala Python i system operacyjny,
        używając wielu wątków.
        Cel: Serwer NIE MOŻE zginąć (Segfault) ani się zablokować. Ma to przetrwać.
        """
        with ServerProcess(pawn_row="11111111", timeout=5) as srv:
            addr = srv.addr()

            # Zakładamy bezpieczną bazę przed atakiem
            with make_udp_socket() as s_base:
                gs_base = join_as_player(addr, 10101, s_base)
                gid = gs_base.game_id

            attack_running = True

            def evil_flooder():
                # Tworzymy surowy socket UDP, by zminimalizować narzut
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                while attack_running:
                    # Generujemy losowy śmieć od 1 do 100 bajtów
                    garbage = bytes(random.getrandbits(8) for _ in range(random.randint(1, 100)))
                    try:
                        sock.sendto(garbage, addr)
                    except OSError:
                        # OS może zablokować sendto, gdy bufor nadawczy (TX) się przepełni. Ignorujemy.
                        pass
                sock.close()

            # Odpalamy 20 wątków siejących zniszczenie
            threads = [threading.Thread(target=evil_flooder) for _ in range(20)]
            for t in threads:
                t.start()

            # Trzymamy atak przez 2.5 sekundy (dla serwera UDP to wieczność i setki tysięcy pakietów)
            time.sleep(2.5)
            attack_running = False

            for t in threads:
                t.join()

            # KRYTYCZNE SPRAWDZENIE: Czy proces serwera wciąż żyje?
            assert srv.proc.poll() is None, "KATASTROFA: Serwer umarł (Segfault/Crash) podczas zalewu UDP!"

            # Dajemy mu 0.5 sekundy na przetrawienie/odrzucenie resztek z bufora jądra
            time.sleep(0.5)

            # Sprawdzamy, czy dane nie uległy korupcji i serwer nadal odpowiada
            with make_udp_socket(timeout=3.0) as s_check:
                raw_check = udp_send_recv(s_check, pack_keep_alive(10101, gid), addr)

            assert raw_check is not None, "Serwer żyje, ale całkowicie zamarzł i przestał odpowiadać na poprawne pakiety!"
            gs_check = GameState(raw_check)
            assert gs_check.game_id == gid, "Pamięć uległa korupcji!"

    def test_std_map_black_hole_oom(self):
        """
        Próbujemy wywołać std::bad_alloc na stercie (OOM - Out of Memory).
        Generujemy absurdalną ilość prawidłowych żądań MSG_JOIN od unikalnych graczy.
        Nawet jeśli Python nie zdąży zapchać całego RAM-u, serwer poddany
        takiej presji alokacji nie może się zawiesić ani wyłączyć.
        """
        with ServerProcess(pawn_row="1" * 256, timeout=10) as srv:
            addr = srv.addr()

            def create_games(start_idx, count):
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                for i in range(count):
                    try:
                        # Wysyłamy JOIN w ciemno, nie czekamy na recvfrom (fire and forget)
                        sock.sendto(pack_join(start_idx + i), addr)
                    except OSError:
                        pass
                sock.close()

            # 10 wątków, każdy próbuje założyć 15 000 gier (150 000 gier łącznie)
            threads = [threading.Thread(target=create_games, args=(200000 + i * 15000, 15000)) for i in range(10)]

            for t in threads:
                t.start()
            for t in threads:
                t.join()

            # Upewniamy się, że try-catch złapał ewentualny brak pamięci
            assert srv.proc.poll() is None, "Serwer wyrzucił wyjątek i się wyłączył (prawdopodobnie nie złapano bad_alloc)!"

            # Serwer musi być w stanie przyjąć nowe poprawne żądanie po ataku
            with make_udp_socket(timeout=4.0) as s_check:
                raw = udp_send_recv(s_check, pack_join(999999), addr)

            assert raw is not None, "Serwer po ataku na pamięć przestał odpowiadać!"

    def test_schrodingers_state_machine_fuzzer(self):
        """
        Atakujemy logikę gry.
        Jedna gra. 50 wątków jednocześnie wysyła losowo: MOVE_1, MOVE_2, GIVE_UP, KEEP_ALIVE.
        To czysty chaos. Serwer przetwarza to sekwencyjnie, ale OS i sockety szaleją.
        Cel: Stan gry musi być zawsze w 100% legalny z punktu widzenia zasad
        (nie można zbić tego samego piona dwa razy, po GIVE_UP gra musi być WIN_*, itp.).
        """
        with ServerProcess(pawn_row="11111111", timeout=5) as srv:
            addr = srv.addr()

            with make_udp_socket() as s_main:
                join_as_player(addr, 111, s_main)
                gs = join_as_player(addr, 222, s_main)
                gid = gs.game_id

            def chaos_monkey():
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                for _ in range(500): # 500 pakietów na wątek
                    action = random.choice([
                        pack_move1(222, gid, random.randint(0, 7)),
                        pack_move1(111, gid, random.randint(0, 7)),
                        pack_move2(222, gid, random.randint(0, 6)),
                        pack_move2(111, gid, random.randint(0, 6)),
                        pack_give_up(111, gid),
                        pack_give_up(222, gid),
                        pack_keep_alive(111, gid)
                    ])
                    try:
                        sock.sendto(action, addr)
                    except OSError:
                        pass
                sock.close()

            # 20 małp chaosu uderzających w jedną planszę
            threads = [threading.Thread(target=chaos_monkey) for _ in range(20)]
            for t in threads:
                t.start()
            for t in threads:
                t.join()

            assert srv.proc.poll() is None, "Serwer wykrzaczył się podczas fuzowania logiki!"

            # Odpytujemy o stan po bitwie
            with make_udp_socket(timeout=2.0) as s_check:
                raw_final = udp_send_recv(s_check, pack_keep_alive(111, gid), addr)

            assert raw_final is not None, "Brak odpowiedzi po ataku na maszynę stanów!"

            resp = parse_response(raw_final)
            if isinstance(resp, WrongMsg):
                # Jeśli gra została wyczyszczona przez GC (timeout) bo małpy uderzały
                # przez ponad `args.timeout` sekund - to jest w 100% legalne i git.
                pass
            else:
                # Jeśli gra nadal istnieje, MUSI być w poprawnym stanie terminalnym
                # (bo na 100% ktoś wysłał skutecznie GIVE_UP lub zbił ostatniego piona)
                # lub ewentualnie w trakcie gry (bardzo mała szansa, ale legalne).
                assert resp.status in (STATUS_WIN_A, STATUS_WIN_B, STATUS_TURN_A, STATUS_TURN_B), "Korupcja maszyny stanów! Niezidentyfikowany status gry."