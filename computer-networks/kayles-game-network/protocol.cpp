#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <ranges>

#include <arpa/inet.h>

#include "protocol.h"
#include "common.h"

constexpr std::size_t MAX_MESSAGE_SIZE = 4;
constexpr std::size_t MIN_MESSAGE_SIZE = 2;
constexpr std::size_t BYTE_SIZE = 8;

static const std::size_t message_sizes[] = {
    MSG_JOIN_LENGTH,
    MSG_MOVE_1_LENGTH,
    MSG_MOVE_2_LENGTH,
    MSG_KEEP_ALIVE_LENGTH,
    MSG_GIVE_UP_LENGTH
};

ParseResult parse_client_message(const std::string& text, ClientMessage& msg) {
    if (text.empty()) {
        return {PARSE_ERR_INPUT, "Empty string"};
    }

    std::vector<uint32_t> text_split;

    for (auto part : text | std::views::split('/')) {
        uint32_t value = 0;
        std::string token(part.begin(), part.end());

        if (!validate_number(token, value)) {
            return {PARSE_ERR_INPUT, "Fields must be non-negative integers"};
        }

        text_split.push_back(value);
    }

    const std::size_t count = text_split.size();

    if (count > MAX_MESSAGE_SIZE) {
        return {PARSE_ERR_INPUT, "Message too long"};
    }

    if (count < MIN_MESSAGE_SIZE) {
        return {PARSE_ERR_INPUT, "Message too short: requires type/player_id"};
    }

    msg.msg_type = static_cast<std::uint8_t>(text_split[0]);
    msg.player_id = text_split[1];
    msg.game_id = 0;
    msg.pawn = 0;

    switch (msg.msg_type) {
        case MSG_JOIN:
            if (count != 2) {
                return {PARSE_ERR_INPUT, "JOIN requires exactly: 0/player_id"};
            }
            break;

        case MSG_MOVE_1:
        case MSG_MOVE_2:
            if (count != 4) {
                return {PARSE_ERR_INPUT, "MOVE requires: type/player_id/game_id/pawn"};
            }
            if (text_split[3] > MAX_UINT8) {
                return {PARSE_ERR_INPUT, "Pawn index out of range (0-255)"};
            }
            msg.game_id = text_split[2];
            msg.pawn = static_cast<std::uint8_t>(text_split[3]);
            break;

        case MSG_KEEP_ALIVE:
        case MSG_GIVE_UP:
            if (count != 3) {
                return {PARSE_ERR_INPUT, "Utility msg requires: type/player_id/game_id"};
            }
            msg.game_id = text_split[2];
            break;

        default:
            return {PARSE_ERR_INPUT, "Unknown message type (0-4)"};
    }

    return {PARSE_OK, "Success"};
}

std::string parse_error_string(ParseResult res) {
    return res.msg;
}

void print_client_message(const ClientMessage& msg) {
    std::cout << "ClientMessage {\n"
              << "  msg_type  : " << static_cast<unsigned>(msg.msg_type) << "\n"
              << "  player_id : " << msg.player_id << "\n"
              << "  game_id   : " << msg.game_id << "\n"
              << "  pawn      : " << static_cast<unsigned>(msg.pawn) << "\n"
              << "}\n";
}

std::size_t serialize_client_message(const ClientMessage& msg, std::vector<std::uint8_t>& buf) {
    const std::size_t size = get_client_message_size(msg);
    if (size == 0) {
        buf.clear();
        return 0;
    }

    buf.assign(size, 0);
    std::size_t offset = 0;

    buf[offset++] = msg.msg_type;

    const uint32_t net_player_id = htonl(msg.player_id);
    std::memcpy(buf.data() + offset, &net_player_id, sizeof(net_player_id));
    offset += sizeof(net_player_id);

    if (msg.msg_type == MSG_MOVE_1 || msg.msg_type == MSG_MOVE_2 ||
        msg.msg_type == MSG_KEEP_ALIVE || msg.msg_type == MSG_GIVE_UP) {
        const uint32_t net_game_id = htonl(msg.game_id);
        std::memcpy(buf.data() + offset, &net_game_id, sizeof(net_game_id));
        offset += sizeof(net_game_id);
    }

    if (msg.msg_type == MSG_MOVE_1 || msg.msg_type == MSG_MOVE_2) {
        buf[offset++] = msg.pawn;
    }

    return offset;
}

std::size_t get_client_message_size(const ClientMessage& msg) {
    constexpr std::size_t count = sizeof(message_sizes) / sizeof(message_sizes[0]);
    if (msg.msg_type >= count) {
        return 0;
    }
    return message_sizes[msg.msg_type];
}

bool deserialize_client_message(ClientMessage& msg,
                                const std::vector<std::uint8_t>& buf,
                                std::uint8_t& error_index) {
    error_index = 0;

    if (buf.empty()) {
        return false;
    }

    std::size_t offset = 0;
    msg.msg_type = buf[offset++];

    switch (msg.msg_type) {
        case MSG_JOIN: {
            if (buf.size() != MSG_JOIN_LENGTH) {
                error_index = static_cast<std::uint8_t>(
                    buf.size() < MSG_JOIN_LENGTH ? buf.size() : MSG_JOIN_LENGTH
                );
                return false;
            }

            uint32_t net_player_id = 0;
            std::memcpy(&net_player_id, buf.data() + offset, sizeof(net_player_id));
            msg.player_id = ntohl(net_player_id);
            msg.game_id = 0;
            msg.pawn = 0;

            if (msg.player_id == 0) {
                error_index = 1;
                return false;
            }

            break;
        }

        case MSG_MOVE_1:
        case MSG_MOVE_2: {
            const std::size_t expected =
                (msg.msg_type == MSG_MOVE_1) ? MSG_MOVE_1_LENGTH : MSG_MOVE_2_LENGTH;

            if (buf.size() != expected) {
                error_index = static_cast<std::uint8_t>(
                    buf.size() < expected ? buf.size() : expected
                );
                return false;
            }

            uint32_t net_player_id = 0;
            uint32_t net_game_id = 0;

            std::memcpy(&net_player_id, buf.data() + offset, sizeof(net_player_id));
            offset += sizeof(net_player_id);

            std::memcpy(&net_game_id, buf.data() + offset, sizeof(net_game_id));
            offset += sizeof(net_game_id);

            msg.player_id = ntohl(net_player_id);
            msg.game_id = ntohl(net_game_id);
            msg.pawn = buf[offset];

            break;
        }

        case MSG_KEEP_ALIVE:
        case MSG_GIVE_UP: {
            const std::size_t expected =
                (msg.msg_type == MSG_KEEP_ALIVE) ? MSG_KEEP_ALIVE_LENGTH : MSG_GIVE_UP_LENGTH;

            if (buf.size() != expected) {
                error_index = static_cast<std::uint8_t>(
                    buf.size() < expected ? buf.size() : expected
                );
                return false;
            }

            uint32_t net_player_id = 0;
            uint32_t net_game_id = 0;

            std::memcpy(&net_player_id, buf.data() + offset, sizeof(net_player_id));
            offset += sizeof(net_player_id);

            std::memcpy(&net_game_id, buf.data() + offset, sizeof(net_game_id));
            offset += sizeof(net_game_id);

            msg.player_id = ntohl(net_player_id);
            msg.game_id = ntohl(net_game_id);
            msg.pawn = 0;

            break;
        }

        default:
            error_index = 0;
            return false;
    }

    return true;
}

void handle_invalid_game(std::uint8_t& error_index) {
    error_index = GAME_ID_INDEX;
}

std::size_t serialize(const GameState& game_state, std::vector<std::uint8_t>& buf) {
    const std::size_t pawn_bytes = (game_state.max_pawn / BYTE_SIZE) + 1;
    const std::size_t total_size = GAME_STATE_HEADER_SIZE + pawn_bytes;

    if (game_state.pawn_row.size() < pawn_bytes) {
        buf.clear();
        return 0;
    }

    buf.assign(total_size, 0);
    std::size_t offset = 0;

    const uint32_t net_game_id = htonl(game_state.game_id);
    std::memcpy(buf.data() + offset, &net_game_id, sizeof(net_game_id));
    offset += sizeof(net_game_id);

    const uint32_t net_player_a_id = htonl(game_state.player_a_id);
    std::memcpy(buf.data() + offset, &net_player_a_id, sizeof(net_player_a_id));
    offset += sizeof(net_player_a_id);

    const uint32_t net_player_b_id = htonl(game_state.player_b_id);
    std::memcpy(buf.data() + offset, &net_player_b_id, sizeof(net_player_b_id));
    offset += sizeof(net_player_b_id);

    buf[offset++] = game_state.status;
    buf[offset++] = game_state.max_pawn;

    std::memcpy(buf.data() + offset, game_state.pawn_row.data(), pawn_bytes);
    offset += pawn_bytes;

    return offset;
}

std::size_t serialize(const WrongMessage& wrong_message, std::vector<std::uint8_t>& buf) {
    const std::size_t total_size = CLIENT_MESSAGE_PREFIX_SIZE + 2;
    buf.assign(total_size, 0);

    std::size_t offset = 0;

    std::memcpy(buf.data() + offset,
                wrong_message.client_message_prefix,
                CLIENT_MESSAGE_PREFIX_SIZE);
    offset += CLIENT_MESSAGE_PREFIX_SIZE;

    buf[offset++] = wrong_message.status;
    buf[offset++] = wrong_message.error_index;

    return offset;
}

std::size_t serialize(const ServerResponse& server_response, std::vector<std::uint8_t>& buf) {
    return std::visit([&buf](const auto& response) -> std::size_t {
        return serialize(response, buf);
    }, server_response.response);
}

bool deserialize(GameState& game_state, const std::vector<std::uint8_t>& buf) {
    if (buf.size() < GAME_STATE_HEADER_SIZE) {
        return false;
    }

    std::size_t offset = 0;

    uint32_t net_game_id = 0;
    uint32_t net_player_a_id = 0;
    uint32_t net_player_b_id = 0;

    std::memcpy(&net_game_id, buf.data() + offset, sizeof(net_game_id));
    game_state.game_id = ntohl(net_game_id);
    offset += sizeof(net_game_id);

    std::memcpy(&net_player_a_id, buf.data() + offset, sizeof(net_player_a_id));
    game_state.player_a_id = ntohl(net_player_a_id);
    offset += sizeof(net_player_a_id);

    std::memcpy(&net_player_b_id, buf.data() + offset, sizeof(net_player_b_id));
    game_state.player_b_id = ntohl(net_player_b_id);
    offset += sizeof(net_player_b_id);

    game_state.status = buf[offset++];
    game_state.max_pawn = buf[offset++];

    const std::size_t pawn_bytes = (game_state.max_pawn / BYTE_SIZE) + 1;

    if (buf.size() < offset + pawn_bytes) {
        return false;
    }

    game_state.pawn_row.resize(pawn_bytes);
    std::memcpy(game_state.pawn_row.data(), buf.data() + offset, pawn_bytes);

    return true;
}

bool deserialize(WrongMessage& wrong_message, const std::vector<std::uint8_t>& buf) {
    if (buf.size() < CLIENT_MESSAGE_PREFIX_SIZE + 2) {
        return false;
    }

    std::size_t offset = 0;

    std::memcpy(wrong_message.client_message_prefix,
                buf.data() + offset,
                CLIENT_MESSAGE_PREFIX_SIZE);
    offset += CLIENT_MESSAGE_PREFIX_SIZE;

    wrong_message.status = buf[offset++];
    wrong_message.error_index = buf[offset++];

    return true;
}

bool deserialize(ServerResponse& msg, const std::vector<std::uint8_t>& buf) {
    if (buf.size() < CLIENT_MESSAGE_PREFIX_SIZE + 1) {
        return false;
    }

    if (buf[CLIENT_MESSAGE_PREFIX_SIZE] == WRONG_MESSAGE_STATUS) {
        WrongMessage wrong_message;
        if (!deserialize(wrong_message, buf)) {
            return false;
        }
        msg.response_type = MSG_WRONG_MESSAGE;
        msg.response = wrong_message;
    } else {
        GameState game_state;
        if (!deserialize(game_state, buf)) {
            return false;
        }
        msg.response_type = MSG_CORRECT_MESSAGE;
        msg.response = game_state;
    }

    return true;
}

void print(const WrongMessage& wrong_message) {
    std::cout << "WrongMessage {\n";
    std::cout << "  client message prefix : ";

    for (std::size_t i = 0; i < CLIENT_MESSAGE_PREFIX_SIZE; ++i) {
        std::cout << static_cast<unsigned>(wrong_message.client_message_prefix[i]) << " ";
    }

    std::cout << "\n";
    std::cout << "  status                : " << static_cast<unsigned>(wrong_message.status) << "\n";
    std::cout << "  error index           : " << static_cast<unsigned>(wrong_message.error_index) << "\n";
    std::cout << "}\n";
}

void print(const GameState& game_state) {
    std::cout << "GameState {\n";
    std::cout << "  game id      : " << game_state.game_id << "\n";
    std::cout << "  player A id  : " << game_state.player_a_id << "\n";
    std::cout << "  player B id  : " << game_state.player_b_id << "\n";
    std::cout << "  status       : " << static_cast<unsigned>(game_state.status) << "\n";
    std::cout << "  max pawn     : " << static_cast<unsigned>(game_state.max_pawn) << "\n";

    std::cout << "  pawn row     : ";
    for (std::size_t pawn = 0; pawn < game_state.max_pawn; ++pawn) {
        uint8_t bit = 0;
        bitset_get(game_state.pawn_row, pawn, bit);
        std::cout << static_cast<unsigned>(bit);
    }
    std::cout << "\n";

    std::cout << "}\n";
}

void print(const ServerResponse& server_response) {
    std::visit([](const auto& response) {
        print(response);
    }, server_response.response);
}