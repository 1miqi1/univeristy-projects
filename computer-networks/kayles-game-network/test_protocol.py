import unittest

# Zmień nazwę modułu na swoją, jeśli jest inna.
import protocol as proto


class TestParseClientMessage(unittest.TestCase):
    def test_parse_join_valid(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message("0/123", msg)

        self.assertEqual(res.code, proto.PARSE_OK)
        self.assertEqual(res.msg, "Success")
        self.assertEqual(msg.msg_type, proto.MSG_JOIN)
        self.assertEqual(msg.player_id, 123)
        self.assertEqual(msg.game_id, 0)
        self.assertEqual(msg.pawn, 0)

    def test_parse_move_1_valid(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message(f"{proto.MSG_MOVE_1}/10/20/7", msg)

        self.assertEqual(res.code, proto.PARSE_OK)
        self.assertEqual(msg.msg_type, proto.MSG_MOVE_1)
        self.assertEqual(msg.player_id, 10)
        self.assertEqual(msg.game_id, 20)
        self.assertEqual(msg.pawn, 7)

    def test_parse_move_2_valid(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message(f"{proto.MSG_MOVE_2}/11/21/8", msg)

        self.assertEqual(res.code, proto.PARSE_OK)
        self.assertEqual(msg.msg_type, proto.MSG_MOVE_2)
        self.assertEqual(msg.player_id, 11)
        self.assertEqual(msg.game_id, 21)
        self.assertEqual(msg.pawn, 8)

    def test_parse_keep_alive_valid(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message(f"{proto.MSG_KEEP_ALIVE}/33/44", msg)

        self.assertEqual(res.code, proto.PARSE_OK)
        self.assertEqual(msg.msg_type, proto.MSG_KEEP_ALIVE)
        self.assertEqual(msg.player_id, 33)
        self.assertEqual(msg.game_id, 44)
        self.assertEqual(msg.pawn, 0)

    def test_parse_give_up_valid(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message(f"{proto.MSG_GIVE_UP}/99/100", msg)

        self.assertEqual(res.code, proto.PARSE_OK)
        self.assertEqual(msg.msg_type, proto.MSG_GIVE_UP)
        self.assertEqual(msg.player_id, 99)
        self.assertEqual(msg.game_id, 100)
        self.assertEqual(msg.pawn, 0)

    def test_parse_empty_string(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message("", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "Empty string")

    def test_parse_non_numeric(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message("0/abc", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "Fields must be non-negative integers")

    def test_parse_negative_like_token(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message("0/-1", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "Fields must be non-negative integers")

    def test_parse_message_too_short(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message("0", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "Message too short: requires type/player_id")

    def test_parse_message_too_long(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message("0/1/2/3/4", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "Message too long")

    def test_parse_player_id_zero(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message("0/0", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "Player index can't be 0")

    def test_parse_join_wrong_field_count(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message("0/1/2", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "JOIN requires exactly: 0/player_id")

    def test_parse_move_wrong_field_count(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message(f"{proto.MSG_MOVE_1}/1/2", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "MOVE requires: type/player_id/game_id/pawn")

    def test_parse_keep_alive_wrong_field_count(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message(f"{proto.MSG_KEEP_ALIVE}/1", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "Utility msg requires: type/player_id/game_id")

    def test_parse_pawn_out_of_range(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message(f"{proto.MSG_MOVE_1}/1/2/256", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "Pawn index out of range (0-255)")

    def test_parse_unknown_message_type(self):
        msg = proto.ClientMessage()
        res = proto.parse_client_message("9/1", msg)

        self.assertEqual(res.code, proto.PARSE_ERR_INPUT)
        self.assertEqual(proto.parse_error_string(res), "Unknown message type (0-4)")


class TestHelperFunctions(unittest.TestCase):
    def test_get_client_message_size_join(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_JOIN
        self.assertEqual(proto.get_client_message_size(msg), proto.MSG_JOIN_LENGTH)

    def test_get_client_message_size_move_1(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_MOVE_1
        self.assertEqual(proto.get_client_message_size(msg), proto.MSG_MOVE_1_LENGTH)

    def test_get_client_message_size_move_2(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_MOVE_2
        self.assertEqual(proto.get_client_message_size(msg), proto.MSG_MOVE_2_LENGTH)

    def test_get_client_message_size_keep_alive(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_KEEP_ALIVE
        self.assertEqual(proto.get_client_message_size(msg), proto.MSG_KEEP_ALIVE_LENGTH)

    def test_get_client_message_size_give_up(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_GIVE_UP
        self.assertEqual(proto.get_client_message_size(msg), proto.MSG_GIVE_UP_LENGTH)

    def test_get_client_message_size_invalid_type(self):
        msg = proto.ClientMessage()
        msg.msg_type = 255
        self.assertEqual(proto.get_client_message_size(msg), 0)

    def test_handle_invalid_game(self):
        # To zależy od tego, jak zrobiłeś binding do uint8_t&.
        # Najczęściej wygodniej wystawić osobną funkcję, która zwraca wartość.
        error_index = 0
        error_index = proto.handle_invalid_game(error_index)
        self.assertEqual(error_index, proto.GAME_ID_INDEX)

    def test_create_wrong_message_prefix_exact(self):
        wm = proto.WrongMessage()
        prefix = bytes(range(proto.CLIENT_MESSAGE_PREFIX_SIZE))
        proto.create_wrong_message(wm, prefix, 7)

        self.assertEqual(list(wm.client_message_prefix), list(prefix))
        self.assertEqual(wm.status, proto.WRONG_MESSAGE_STATUS)
        self.assertEqual(wm.error_index, 7)

    def test_create_wrong_message_prefix_short_zero_padded(self):
        wm = proto.WrongMessage()
        prefix = bytes([1, 2, 3])
        proto.create_wrong_message(wm, prefix, 5)

        expected = [1, 2, 3] + [0] * (proto.CLIENT_MESSAGE_PREFIX_SIZE - 3)
        self.assertEqual(list(wm.client_message_prefix), expected)
        self.assertEqual(wm.status, proto.WRONG_MESSAGE_STATUS)
        self.assertEqual(wm.error_index, 5)


class TestSerializeClientMessage(unittest.TestCase):
    def test_serialize_join(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_JOIN
        msg.player_id = 0x01020304
        msg.game_id = 0
        msg.pawn = 0

        buf = bytearray(proto.MSG_JOIN_LENGTH)
        written = proto.serialize_client_message(msg, buf)

        self.assertEqual(written, proto.MSG_JOIN_LENGTH)
        self.assertEqual(buf[0], proto.MSG_JOIN)
        self.assertEqual(bytes(buf[1:5]), b"\x01\x02\x03\x04")

    def test_serialize_move_1(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_MOVE_1
        msg.player_id = 1
        msg.game_id = 2
        msg.pawn = 9

        buf = bytearray(proto.MSG_MOVE_1_LENGTH)
        written = proto.serialize_client_message(msg, buf)

        self.assertEqual(written, proto.MSG_MOVE_1_LENGTH)
        self.assertEqual(buf[0], proto.MSG_MOVE_1)
        self.assertEqual(bytes(buf[1:5]), b"\x00\x00\x00\x01")
        self.assertEqual(bytes(buf[5:9]), b"\x00\x00\x00\x02")
        self.assertEqual(buf[9], 9)

    def test_serialize_move_2(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_MOVE_2
        msg.player_id = 5
        msg.game_id = 6
        msg.pawn = 7

        buf = bytearray(proto.MSG_MOVE_2_LENGTH)
        written = proto.serialize_client_message(msg, buf)

        self.assertEqual(written, proto.MSG_MOVE_2_LENGTH)
        self.assertEqual(buf[0], proto.MSG_MOVE_2)
        self.assertEqual(bytes(buf[1:5]), b"\x00\x00\x00\x05")
        self.assertEqual(bytes(buf[5:9]), b"\x00\x00\x00\x06")
        self.assertEqual(buf[9], 7)

    def test_serialize_keep_alive(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_KEEP_ALIVE
        msg.player_id = 10
        msg.game_id = 20
        msg.pawn = 0

        buf = bytearray(proto.MSG_KEEP_ALIVE_LENGTH)
        written = proto.serialize_client_message(msg, buf)

        self.assertEqual(written, proto.MSG_KEEP_ALIVE_LENGTH)
        self.assertEqual(buf[0], proto.MSG_KEEP_ALIVE)
        self.assertEqual(bytes(buf[1:5]), b"\x00\x00\x00\x0a")
        self.assertEqual(bytes(buf[5:9]), b"\x00\x00\x00\x14")

    def test_serialize_give_up(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_GIVE_UP
        msg.player_id = 10
        msg.game_id = 20
        msg.pawn = 0

        buf = bytearray(proto.MSG_GIVE_UP_LENGTH)
        written = proto.serialize_client_message(msg, buf)

        self.assertEqual(written, proto.MSG_GIVE_UP_LENGTH)
        self.assertEqual(buf[0], proto.MSG_GIVE_UP)
        self.assertEqual(bytes(buf[1:5]), b"\x00\x00\x00\x0a")
        self.assertEqual(bytes(buf[5:9]), b"\x00\x00\x00\x14")

    def test_serialize_client_message_buffer_too_small(self):
        msg = proto.ClientMessage()
        msg.msg_type = proto.MSG_JOIN
        msg.player_id = 1

        buf = bytearray(proto.MSG_JOIN_LENGTH - 1)
        written = proto.serialize_client_message(msg, buf)

        self.assertEqual(written, 0)

    def test_serialize_client_message_invalid_type(self):
        msg = proto.ClientMessage()
        msg.msg_type = 255
        msg.player_id = 1

        buf = bytearray(32)
        written = proto.serialize_client_message(msg, buf)

        self.assertEqual(written, 0)


class TestDeserializeClientMessage(unittest.TestCase):
    def test_deserialize_join_valid(self):
        raw = bytes([
            proto.MSG_JOIN,
            0x00, 0x00, 0x00, 0x05
        ])

        msg = proto.ClientMessage()
        error_index = 0
        ok, error_index = proto.deserialize_client_message(msg, raw, len(raw), error_index)

        self.assertTrue(ok)
        self.assertEqual(error_index, 0)
        self.assertEqual(msg.msg_type, proto.MSG_JOIN)
        self.assertEqual(msg.player_id, 5)
        self.assertEqual(msg.game_id, 0)
        self.assertEqual(msg.pawn, 0)

    def test_deserialize_join_wrong_length(self):
        raw = bytes([proto.MSG_JOIN, 0x00, 0x00, 0x00])

        msg = proto.ClientMessage()
        error_index = 0
        ok, error_index = proto.deserialize_client_message(msg, raw, len(raw), error_index)

        self.assertFalse(ok)
        self.assertEqual(error_index, min(len(raw), proto.MSG_JOIN_LENGTH))

    def test_deserialize_join_player_zero(self):
        raw = bytes([
            proto.MSG_JOIN,
            0x00, 0x00, 0x00, 0x00
        ])

        msg = proto.ClientMessage()
        error_index = 0
        ok, error_index = proto.deserialize_client_message(msg, raw, len(raw), error_index)

        self.assertFalse(ok)
        self.assertEqual(error_index, 1)

    def test_deserialize_move_1_valid(self):
        raw = bytes([
            proto.MSG_MOVE_1,
            0x00, 0x00, 0x00, 0x03,
            0x00, 0x00, 0x00, 0x09,
            0x11
        ])

        msg = proto.ClientMessage()
        error_index = 0
        ok, error_index = proto.deserialize_client_message(msg, raw, len(raw), error_index)

        self.assertTrue(ok)
        self.assertEqual(error_index, 0)
        self.assertEqual(msg.player_id, 3)
        self.assertEqual(msg.game_id, 9)
        self.assertEqual(msg.pawn, 0x11)

    def test_deserialize_move_2_valid(self):
        raw = bytes([
            proto.MSG_MOVE_2,
            0x00, 0x00, 0x00, 0x07,
            0x00, 0x00, 0x00, 0x15,
            0x02
        ])

        msg = proto.ClientMessage()
        error_index = 0
        ok, error_index = proto.deserialize_client_message(msg, raw, len(raw), error_index)

        self.assertTrue(ok)
        self.assertEqual(error_index, 0)
        self.assertEqual(msg.player_id, 7)
        self.assertEqual(msg.game_id, 21)
        self.assertEqual(msg.pawn, 2)

    def test_deserialize_keep_alive_valid(self):
        raw = bytes([
            proto.MSG_KEEP_ALIVE,
            0x00, 0x00, 0x00, 0x08,
            0x00, 0x00, 0x00, 0x0A
        ])

        msg = proto.ClientMessage()
        error_index = 0
        ok, error_index = proto.deserialize_client_message(msg, raw, len(raw), error_index)

        self.assertTrue(ok)
        self.assertEqual(error_index, 0)
        self.assertEqual(msg.player_id, 8)
        self.assertEqual(msg.game_id, 10)
        self.assertEqual(msg.pawn, 0)

    def test_deserialize_give_up_valid(self):
        raw = bytes([
            proto.MSG_GIVE_UP,
            0x00, 0x00, 0x00, 0x08,
            0x00, 0x00, 0x00, 0x0A
        ])

        msg = proto.ClientMessage()
        error_index = 0
        ok, error_index = proto.deserialize_client_message(msg, raw, len(raw), error_index)

        self.assertTrue(ok)
        self.assertEqual(error_index, 0)
        self.assertEqual(msg.player_id, 8)
        self.assertEqual(msg.game_id, 10)
        self.assertEqual(msg.pawn, 0)

    def test_deserialize_zero_received_length(self):
        raw = b""
        msg = proto.ClientMessage()
        error_index = 0

        ok, error_index = proto.deserialize_client_message(msg, raw, 0, error_index)
        self.assertFalse(ok)

    def test_deserialize_unknown_type(self):
        raw = bytes([99, 0, 0, 0, 1])

        msg = proto.ClientMessage()
        error_index = 0
        ok, error_index = proto.deserialize_client_message(msg, raw, len(raw), error_index)

        self.assertFalse(ok)


class TestGameStateSerialization(unittest.TestCase):
    def test_serialize_deserialize_game_state_roundtrip(self):
        gs = proto.GameState()
        gs.game_id = 100
        gs.player_a_id = 11
        gs.player_b_id = 22
        gs.status = 3
        gs.max_pawn = 10
        gs.pawn_row = bytes([0b10101010, 0b11000000])

        total_size = proto.GAME_STATE_HEADER_SIZE + ((gs.max_pawn // 8) + 1)
        buf = bytearray(total_size)

        written = proto.serialize(gs, buf)
        self.assertEqual(written, total_size)

        out = proto.GameState()
        ok = proto.deserialize(out, bytes(buf))
        self.assertTrue(ok)

        self.assertEqual(out.game_id, gs.game_id)
        self.assertEqual(out.player_a_id, gs.player_a_id)
        self.assertEqual(out.player_b_id, gs.player_b_id)
        self.assertEqual(out.status, gs.status)
        self.assertEqual(out.max_pawn, gs.max_pawn)
        self.assertEqual(bytes(out.pawn_row), bytes(gs.pawn_row))

    def test_serialize_game_state_buffer_too_small(self):
        gs = proto.GameState()
        gs.game_id = 1
        gs.player_a_id = 2
        gs.player_b_id = 3
        gs.status = 1
        gs.max_pawn = 8
        gs.pawn_row = bytes([0xFF, 0x00])

        buf = bytearray(proto.GAME_STATE_HEADER_SIZE)
        written = proto.serialize(gs, buf)

        self.assertEqual(written, 0)

    def test_deserialize_game_state_too_short_header(self):
        gs = proto.GameState()
        ok = proto.deserialize(gs, b"\x00\x01")
        self.assertFalse(ok)

    def test_deserialize_game_state_too_short_pawn_row(self):
        raw = (
            b"\x00\x00\x00\x01"  # game_id
            b"\x00\x00\x00\x02"  # player_a_id
            b"\x00\x00\x00\x03"  # player_b_id
            b"\x01"              # status
            b"\x10"              # max_pawn = 16 -> potrzebne 3 bajty
            b"\xAA"              # tylko 1 bajt
        )

        gs = proto.GameState()
        ok = proto.deserialize(gs, raw)
        self.assertFalse(ok)


class TestWrongMessageSerialization(unittest.TestCase):
    def test_serialize_deserialize_wrong_message_roundtrip(self):
        wm = proto.WrongMessage()
        wm.client_message_prefix = bytes(range(proto.CLIENT_MESSAGE_PREFIX_SIZE))
        wm.status = proto.WRONG_MESSAGE_STATUS
        wm.error_index = 6

        total_size = proto.CLIENT_MESSAGE_PREFIX_SIZE + 2
        buf = bytearray(total_size)

        written = proto.serialize(wm, buf)
        self.assertEqual(written, total_size)

        out = proto.WrongMessage()
        ok = proto.deserialize(out, bytes(buf))
        self.assertTrue(ok)

        self.assertEqual(list(out.client_message_prefix), list(wm.client_message_prefix))
        self.assertEqual(out.status, wm.status)
        self.assertEqual(out.error_index, wm.error_index)

    def test_serialize_wrong_message_buffer_too_small(self):
        wm = proto.WrongMessage()
        wm.client_message_prefix = bytes(range(proto.CLIENT_MESSAGE_PREFIX_SIZE))
        wm.status = proto.WRONG_MESSAGE_STATUS
        wm.error_index = 3

        buf = bytearray(proto.CLIENT_MESSAGE_PREFIX_SIZE + 1)
        written = proto.serialize(wm, buf)

        self.assertEqual(written, 0)

    def test_deserialize_wrong_message_too_short(self):
        wm = proto.WrongMessage()
        ok = proto.deserialize(wm, b"\x00\x01")
        self.assertFalse(ok)


class TestServerResponseSerialization(unittest.TestCase):
    def test_deserialize_server_response_as_wrong_message(self):
        wm = proto.WrongMessage()
        wm.client_message_prefix = bytes(range(proto.CLIENT_MESSAGE_PREFIX_SIZE))
        wm.status = proto.WRONG_MESSAGE_STATUS
        wm.error_index = 4

        buf = bytearray(proto.CLIENT_MESSAGE_PREFIX_SIZE + 2)
        written = proto.serialize(wm, buf)
        self.assertGreater(written, 0)

        resp = proto.ServerResponse()
        ok = proto.deserialize(resp, bytes(buf))

        self.assertTrue(ok)
        self.assertEqual(resp.response_type, proto.MSG_WRONG_MESSAGE)

    def test_deserialize_server_response_as_game_state(self):
        gs = proto.GameState()
        gs.game_id = 7
        gs.player_a_id = 1
        gs.player_b_id = 2
        gs.status = 2
        gs.max_pawn = 3
        gs.pawn_row = bytes([0b11100000])

        total_size = proto.GAME_STATE_HEADER_SIZE + ((gs.max_pawn // 8) + 1)
        buf = bytearray(total_size)
        written = proto.serialize(gs, buf)
        self.assertGreater(written, 0)

        resp = proto.ServerResponse()
        ok = proto.deserialize(resp, bytes(buf))

        self.assertTrue(ok)
        self.assertEqual(resp.response_type, proto.MSG_CORRECT_MESSAGE)

    def test_deserialize_server_response_too_short(self):
        resp = proto.ServerResponse()
        ok = proto.deserialize(resp, b"\x00")
        self.assertFalse(ok)


if __name__ == "__main__":
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